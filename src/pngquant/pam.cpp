/**
 ** Copyright (C) 1989, 1991 by Jef Poskanzer.
 ** Copyright (C) 1997, 2000, 2002 by Greg Roelofs; based on an idea by
 **                                Stefan Schneider.
 ** (C) 2011 by Kornel Lesinski.
 **
 ** Permission to use, copy, modify, and distribute this software and its
 ** documentation for any purpose and without fee is hereby granted, provided
 ** that the above copyright notice appear in all copies and that both that
 ** copyright notice and this permission notice appear in supporting
 ** documentation.  This software is provided "as is" without express or
 ** implied warranty.
 */

#include <stdlib.h>
#include <string.h>
#include "pam.h"

#include "mempool.h"

#define HASH_SIZE ((1<<19) / sizeof(acolorhist_arr_head) - 17)

static
histogram* pam_acolorhashtoacolorhist(
	acolorhash_table* acht,
	uint hist_size,
	double gamma
	)
{
	histogram* hist = (histogram*) malloc(sizeof(hist[0]));
	hist->achv = (hist_item*) malloc(hist_size * sizeof(hist->achv[0]));
	hist->size = hist_size;

	/* Loop through the hash table. */
	double total_weight=0;
	for (uint j=0, i=0; i<HASH_SIZE; ++i) {
		acolorhist_arr_head* achl = &acht->buckets[i];
		if (achl->used) {
			hist->achv[j].acolor = to_f(gamma, achl->color1.rgb);
			hist->achv[j].adjusted_weight = hist->achv[j].perceptual_weight = achl->perceptual_weight1;
			total_weight += achl->perceptual_weight1;
			++j;

			if (achl->used > 1) {
				hist->achv[j].acolor = to_f(gamma, achl->color2.rgb);
				hist->achv[j].adjusted_weight = hist->achv[j].perceptual_weight = achl->perceptual_weight2;
				total_weight += achl->perceptual_weight2;
				++j;

				acolorhist_arr_item* a = achl->other_items;
				for (uint i=0; i<achl->used-2; i++) {
					hist->achv[j].acolor = to_f(gamma, a[i].color.rgb);
					hist->achv[j].adjusted_weight = hist->achv[j].perceptual_weight = a[i].perceptual_weight;
					total_weight += a[i].perceptual_weight;
					++j;
				}
			}
		}
	}

	hist->total_perceptual_weight = total_weight;
	/* All done. */
	return hist;
}

static
acolorhash_table* pam_allocacolorhash()
{
	mempool* m = NULL;
	acolorhash_table* t = (acolorhash_table*) mempool_new(m, sizeof(*t));
	t->buckets = (acolorhist_arr_head*) mempool_new(m, HASH_SIZE * sizeof(t->buckets[0]));
	t->mempool = m;
	return t;
}

static
void pam_freeacolorhash(acolorhash_table* acht)
{
	mempool_free(acht->mempool);
}

static
acolorhash_table* pam_computeacolorhash(
	const rgb_pixel*const* apixels,
	uint cols,
	uint rows,
	double gamma,
	uint maxacolors,
	uint ignorebits,
	const double* importance_map,
	uint* acolorsP
	)
{
	const uint channel_mask = 255>>ignorebits<<ignorebits;
	const uint channel_hmask = (255>>(ignorebits)) ^ 0xFF;
	const uint posterize_mask = channel_mask << 24 | channel_mask << 16 | channel_mask << 8 | channel_mask;
	const uint posterize_high_mask = channel_hmask << 24 | channel_hmask << 16 | channel_hmask << 8 | channel_hmask;

	acolorhash_table* acht = pam_allocacolorhash();
	acolorhist_arr_head* const buckets = acht->buckets;

	uint colors=0;
	
	const uint stacksize = 512;
	acolorhist_arr_item* freestack[stacksize];
	uint freestackp=0;

	/* Go through the entire image, building a hash table of colors. */
	for (uint row=0; row<rows; ++row) {

		double boost=1.0;
		for (uint col=0; col<cols; ++col) {
			if (importance_map) {
				boost = 0.5f+*importance_map++;
			}
			
			// RGBA color is casted to long for easier hasing/comparisons
			rgb_as_long px = {apixels[row][col]};
			unsigned long hash;
			if (!px.rgb.a) {
				// "dirty alpha" has different RGBA values that end up being the same fully transparent color
				px.l=0; hash=0;
			}else {
				// mask posterizes all 4 channels in one go
				px.l = (px.l & posterize_mask) | ((px.l & posterize_high_mask) >> (8-ignorebits));
				// fancier hashing algorithms didn't improve much
				hash = px.l % HASH_SIZE;
			}
			
			/* head of the hash function stores first 2 colors inline (achl->used = 1..2),
			   to reduce number of allocations of achl->other_items.
			 */
			acolorhist_arr_head* achl = &buckets[hash];
			if (achl->color1.l == px.l && achl->used) {
				achl->perceptual_weight1 += boost;
				continue;
			}
			if (achl->used) {
				if (achl->used > 1) {
					if (achl->color2.l == px.l) {
						achl->perceptual_weight2 += boost;
						continue;
					}
					// other items are stored as an array (which gets reallocated if needed)
					acolorhist_arr_item* other_items = achl->other_items;
					uint i = 0;
					for (; i<achl->used-2; i++) {
						if (other_items[i].color.l == px.l) {
							other_items[i].perceptual_weight += boost;
							goto continue_outer_loop;
						}
					}
					
					// the array was allocated with spare items
					if (i < achl->capacity) {
						acolorhist_arr_item item;
						item.color = px;
						item.perceptual_weight = boost;
						other_items[i] = item;
						achl->used++;
						++colors;
						continue;
					}
						
					if (++colors > maxacolors) {
						pam_freeacolorhash(acht);
						return NULL;
					}

					acolorhist_arr_item* new_items;
					uint capacity;
					if (!other_items) { // there was no array previously, alloc "small" array
						capacity = 8;
						if (freestackp <= 0) {
							new_items = (acolorhist_arr_item*) mempool_new(acht->mempool, sizeof(acolorhist_arr_item)*capacity);
						}else {
							// freestack stores previously freed (reallocated) arrays that can be reused
							// (all pesimistically assumed to be capacity = 8)
							new_items = freestack[--freestackp];
						}
					}else {
						// simply reallocs and copies array to larger capacity
						capacity = achl->capacity*2 + 16;
						if (freestackp < stacksize-1) {
							freestack[freestackp++] = other_items;
						}
						new_items = (acolorhist_arr_item*) mempool_new(acht->mempool, sizeof(acolorhist_arr_item)*capacity);
						memcpy(new_items, other_items, sizeof(other_items[0])*achl->capacity);
					}
					achl->other_items = new_items;
					achl->capacity = capacity;
					acolorhist_arr_item item;
					item.color = px;
					item.perceptual_weight = boost;
					new_items[i] = item;
					achl->used++;
				}else {
					// these are elses for first checks whether first and second inline-stored colors are used
					achl->color2.l = px.l;
					achl->perceptual_weight2 = boost;
					achl->used = 2;
					++colors;
				}
			}else {
				achl->color1.l = px.l;
				achl->perceptual_weight1 = boost;
				achl->used = 1;
				++colors;
			}
			continue_outer_loop:;
		}

	}
	*acolorsP = colors;
	return acht;
}

void pam_freeacolorhist(histogram* hist)
{
	free(hist->achv);
	free(hist);
}

/* libpam3.c - pam (portable alpha map) utility library part 3
 **
 ** Colormap routines.
 **
 ** Copyright (C) 1989, 1991 by Jef Poskanzer.
 ** Copyright (C) 1997 by Greg Roelofs.
 **
 ** Permission to use, copy, modify, and distribute this software and its
 ** documentation for any purpose and without fee is hereby granted, provided
 ** that the above copyright notice appear in all copies and that both that
 ** copyright notice and this permission notice appear in supporting
 ** documentation.	This software is provided "as is" without express or
 ** implied warranty.
 */

/**
 * Builds color histogram no larger than maxacolors. Ignores (posterizes) ignorebits lower bits in each color.
 * perceptual_weight of each entry is increased by value from importance_map
 */
histogram* pam_computeacolorhist(
	const rgb_pixel*const apixels[],
	uint cols,
	uint rows,
	double gamma,
	uint maxacolors,
	uint ignorebits,
	const double* importance_map
	)
{
	acolorhash_table* acht;
	histogram* hist;

	uint hist_size=0;
	acht = pam_computeacolorhash(apixels, cols, rows, gamma, maxacolors, ignorebits, importance_map, &hist_size);
	if (!acht) return 0;

	hist = pam_acolorhashtoacolorhist(acht, hist_size, gamma);
	pam_freeacolorhash(acht);
	return hist;
}

colormap* pam_colormap(uint colors)
{
	colormap* map = (colormap*) malloc(sizeof(colormap));
	map->palette = (colormap_item*) calloc(colors, sizeof(map->palette[0]));
	map->subset_palette = NULL;
	map->colors = colors;
	return map;
}

void pam_freecolormap(colormap* c)
{
	if (c->subset_palette) pam_freecolormap(c->subset_palette);
	free(c->palette); free(c);
}

