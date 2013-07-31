/*
 **
 ** Copyright (C) 1989, 1991 by Jef Poskanzer.
 ** Copyright (C) 1997, 2000, 2002 by Greg Roelofs; based on an idea by
 **								   Stefan Schneider.
 ** (C) 2011 by Kornel Lesinski.
 **
 ** Permission to use, copy, modify, and distribute this software and its
 ** documentation for any purpose and without fee is hereby granted, provided
 ** that the above copyright notice appear in all copies and that both that
 ** copyright notice and this permission notice appear in supporting
 ** documentation.	This software is provided "as is" without express or
 ** implied warranty.
 */

#include <stdlib.h>
#include <stddef.h>

#include <float.h>

#include "pam.h"
#include "mediancut.h"

#include <vector>
#include <algorithm>

#define index_of_channel(ch) (offsetof(f_pixel,ch)/sizeof(double))

static
f_pixel averagepixels(
	uint clrs,
	const hist_item achv[],
	double min_opaque_val
	)
{
	f_pixel csum(0,0,0,0);
	double sum = 0.0;
	double maxa = 0.0;
	for (uint i=0; i<clrs; ++i) {
		double weight = 1.0;
		const hist_item& hist = achv[i];
		const f_pixel px = hist.acolor;
		/* give more weight to colors that are further away from average
		 this is intended to prevent desaturation of images and fading of whites
		 */
		f_pixel tmp = f_pixel(0.5, 0.5, 0.5, 0.5) - px;
		tmp.square();
		weight += tmp.r + tmp.g + tmp.b;
		weight *= hist.adjusted_weight;
		sum += weight;
		csum += px * weight;
		
		/* find if there are opaque colors, in case we're supposed to preserve opacity exactly (ie_bug) */
		maxa = max(maxa, px.alpha);
	}
	
	/* Colors are in premultiplied alpha colorspace, so they'll blend OK
	 even if different opacities were mixed together */
	if (!sum) sum = 1.0;
	csum /= sum;

	assert(!isnan(csum.r) && !isnan(csum.g) && !isnan(csum.b) && !isnan(csum.alpha));
	
	/** if there was at least one completely opaque color, "round" final color to opaque */
	if (csum.alpha >= min_opaque_val && maxa >= (255.0/256.0)) csum.alpha = 1.0;
	
	return csum;
}

struct box {
	f_pixel color;
	f_pixel variance;
	double sum, total_error;
	uint ind;
	uint colors;
};

/** Weighted per-channel variance of the box. It's used to decide which channel to split by */
static f_pixel box_variance(const hist_item achv[], const box* box)
{
	f_pixel mean = box->color;
	f_pixel variance(0.0, 0.0, 0.0, 0.0);
	f_pixel goodEnough(2.0/256.0, 1.0/256.0, 1.0/256.0, 1.0/256.0);
	goodEnough.square();
	for (uint i=0; i<box->colors; ++i) {
		f_pixel px = achv[box->ind + i].acolor;
		double weight = achv[box->ind + i].adjusted_weight;
		f_pixel diff = mean - px;
		diff.square();
		if (diff.alpha < goodEnough.alpha) diff.alpha *= 0.5;
		if (diff.r < goodEnough.r) diff.r *= 0.5;
		if (diff.g < goodEnough.g) diff.g *= 0.5;
		if (diff.b < goodEnough.b) diff.b *= 0.5;
		variance += diff * weight;
	}
	variance *= f_pixel(4.0/16.0, 7.0/16.0, 9.0/16.0, 5.0/16.0);
	return variance;
}

static inline
double color_weight(f_pixel median, const hist_item& h)
{
	double diff = colordifference(median, h.acolor);
	// if color is "good enough", don't split further
	if (diff < 1.0/256.0/256.0) diff /= 2.0;
	return sqrt(diff) * (sqrt(1.0+h.adjusted_weight)-1.0);
}


static inline void hist_item_swap(hist_item* l, hist_item* r)
{
	if (l != r) {
		hist_item t = *l;
		*l = *r;
		*r = t;
	}
}

inline static
uint qsort_pivot(const hist_item* const base, const uint len)
{
	if (len < 32) return len/2;
	
	const uint aidx=8, bidx=len/2, cidx=len-1;
	const unsigned long a=base[aidx].sort_value, b=base[bidx].sort_value, c=base[cidx].sort_value;
	return (a < b) ? ((b < c) ? bidx : ((a < c) ? cidx : aidx ))
				   : ((b > c) ? bidx : ((a < c) ? aidx : cidx ));
}

inline static
uint qsort_partition(hist_item* const base, const uint len)
{
	uint l = 1, r = len;
	if (len >= 8) {
		hist_item_swap(&base[0], &base[qsort_pivot(base,len)]);
	}
	
	const unsigned long pivot_value = base[0].sort_value;
	while (l < r) {
		if (base[l].sort_value >= pivot_value) {
			l++;
		}else {
			while (l < --r && base[r].sort_value <= pivot_value) {}
			hist_item_swap(&base[l], &base[r]);
		}
	}
	l--;
	hist_item_swap(&base[0], &base[l]);

	return l;
}

/** this is a simple qsort that completely sorts only elements between sort_start and +sort_len. Used to find median of the set. */
static
void hist_item_sort_range(hist_item* base, uint len, int sort_start, const uint sort_len)
{
	do {
		const uint l = qsort_partition(base, len), r = l+1;
		
		if (sort_start+sort_len > 0 && (signed)l >= sort_start && l > 0) {
			hist_item_sort_range(base, l, sort_start, sort_len);
		}
		if (len > r && r < sort_start+sort_len && (signed)len > sort_start) {
			base += r; len -= r; sort_start -= r; // tail-recursive "call"
		}else {
			return;
		}
	} while(1);
}

/** sorts array to make sum of weights lower than halfvar one side, returns edge between <halfvar and >halfvar parts of the set */
static
hist_item* hist_item_sort_halfvar(hist_item* base, uint len, double* const lowervar, const double halfvar)
{
	do {
		const uint l = qsort_partition(base, len), r = l+1;

		// check if sum of left side is smaller than half,
		// if it is, then it doesn't need to be sorted
		uint t = 0; double tmpsum = *lowervar;
		while (t <= l && tmpsum < halfvar) tmpsum += base[t++].color_weight;
		
		if (tmpsum < halfvar) {
			*lowervar = tmpsum;
		}else {
			if (l > 0) {
				hist_item *res = hist_item_sort_halfvar(base, l, lowervar, halfvar);
				if (res) return res;
			}else {
				// End of left recursion. This will be executed in order from the first element.
				*lowervar += base[0].color_weight;
				if (*lowervar > halfvar) return &base[0];
			}
		}
		if (len > r) {
			base += r; len -= r; // tail-recursive "call"
		}else {
			*lowervar += base[r].color_weight;
			return (*lowervar > halfvar) ? &base[r] : NULL;
		}
	} while(1);
}

/** finds median in unsorted set by sorting only minimum required */
static
f_pixel get_median(const box* b, hist_item achv[])
{
	const uint median_start = (b->colors-1)/2;

	hist_item_sort_range(&(achv[b->ind]), b->colors,
					median_start,
					b->colors&1 ? 1 : 2);

	if (b->colors&1) return achv[b->ind + median_start].acolor;
	return averagepixels(2, &achv[b->ind + median_start], 1.0);
}

struct channelvariance {
	channelvariance(uint ch, double var)
		:
		chan(ch),
		variance(var)
	{
	}

	channelvariance() {}
	uint chan;
	double variance;
};

// descend sort
static inline
bool operator < (const channelvariance& ch1, const channelvariance& ch2)
{
	return ch1.variance > ch2.variance;
}

/** Finds which channels need to be sorted first and preproceses achv for fast sort */
static
double prepare_sort(box* b, hist_item achv[])
{
	/*
	 ** Sort dimensions by their variance, and then sort colors first by dimension with highest variance
	 */
	channelvariance channels[4] = {
		channelvariance(index_of_channel(r), b->variance.r),
		channelvariance(index_of_channel(g), b->variance.g),
		channelvariance(index_of_channel(b), b->variance.b),
		channelvariance(index_of_channel(alpha), b->variance.alpha),
	};
	
	std::sort(channels, channels+4);
	
	for (uint i=0; i<b->colors; i++) {
		const double* chans = (const double*)&achv[b->ind + i].acolor;
		// Only the first channel really matters. When trying median cut many times
		// with different histogram weights, I don't want sort randomness to influence outcome.
		achv[b->ind + i].sort_value = ((unsigned long)(chans[channels[0].chan]*65535.0)<<16) |
									   (unsigned long)((chans[channels[2].chan] + chans[channels[1].chan]/2.0 + chans[channels[3].chan]/4.0)*65535.0);
	}
	
	const f_pixel median = get_median(b, achv);
	
	// box will be split to make color_weight of each side even
	const uint ind = b->ind, end = ind+b->colors;
	double totalvar = 0;
	for (uint j=ind; j<end; j++) {
		totalvar += (achv[j].color_weight = color_weight(median, achv[j]));
	}
	return totalvar / 2.0;
}

/*
 ** Find the best splittable box. -1 if no boxes are splittable.
 */
static
int best_splittable_box(const box* bv, uint boxes)
{
	int bi = -1;
	double maxsum = 0;
	for (uint i=0; i<boxes; i++) {
		const box& b = bv[i];
		if (b.colors < 2) continue;

		// looks only at max variance, because it's only going to split by it
		const double cv = max(b.variance.r, b.variance.g, b.variance.b);
		const double thissum = b.sum * max(b.variance.alpha, cv);
		if (thissum > maxsum) {
			maxsum = thissum;
			bi = i;
		}
	}
	return bi;
}

static
colormap* colormap_from_boxes(
	const box* bv,
	uint boxes,
	const hist_item* achv,
	double min_opaque_val
	)
{
	/*
	 ** Ok, we've got enough boxes.	 Now choose a representative color for
	 ** each box.  There are a number of possible ways to make this choice.
	 ** One would be to choose the center of the box; this ignores any structure
	 ** within the boxes.  Another method would be to average all the colors in
	 ** the box - this is the method specified in Heckbert's paper.
	 */

	colormap* map = pam_colormap(boxes);
	
	for (uint bi=0; bi<boxes; ++bi) {
		colormap_item& cm = map->palette[bi];
		const box& bx = bv[bi];
		cm.acolor = bx.color;
		
		/* store total color popularity (perceptual_weight is approximation of it) */
		cm.popularity = 0;
		for (uint i=bx.ind; i<bx.ind+bx.colors; i++) {
			cm.popularity += achv[i].perceptual_weight;
		}
	}

	return map;
}

/* increase histogram popularity by difference from the final color (this is used as part of feedback loop) */
static
void adjust_histogram(
	hist_item* achv,
	const colormap* map,
	const box* bv,
	uint boxes
	)
{
	for (uint bi=0; bi<boxes; ++bi) {
		const box& bx = bv[bi];
		f_pixel pc = map->palette[bi].acolor;
		for (uint i=bx.ind; i<bx.ind+bx.colors; i++) {
			hist_item& hist = achv[i];
			hist.adjusted_weight *= sqrt(1.0 + colordifference(pc, hist.acolor) / 2.0);
		}
	}
}

double box_error(const box* box, const hist_item achv[])
{
	f_pixel avg = box->color;

	double total_error=0;
	for (int i=0; i<box->colors; ++i) {
		total_error += colordifference(avg, achv[box->ind + i].acolor) * achv[box->ind + i].perceptual_weight;
	}

	return total_error;
}


static
int total_box_error_below_target(double target_mse, box bv[], uint boxes, const histogram *hist)
{
	target_mse *= hist->total_perceptual_weight;
	double total_error=0;
	for (int i=0; i<boxes; i++) {
		// error is (re)calculated lazily
		if (bv[i].total_error >= 0) {
			total_error += bv[i].total_error;
		}
		if (total_error > target_mse) return 0;
	}

	for (int i=0; i<boxes; i++) {
		if (bv[i].total_error < 0) {
			bv[i].total_error = box_error(&bv[i], hist->achv);
			total_error += bv[i].total_error;
		}
		if (total_error > target_mse) return 0;
	}

	return 1;
}


/*
 ** Here is the fun part, the median-cut colormap generator.  This is based
 ** on Paul Heckbert's paper, "Color Image Quantization for Frame Buffer
 ** Display," SIGGRAPH 1982 Proceedings, page 297.
 */
colormap* mediancut(histogram* hist, const double min_opaque_val, uint newcolors, const double target_mse)
{
	hist_item* achv = hist->achv;
	box* bv = new box[newcolors];

	/*
	 ** Set up the initial box.
	 */
	bv[0].ind = 0;
	bv[0].colors = hist->size;
	bv[0].color = averagepixels(bv[0].colors, &achv[bv[0].ind], min_opaque_val);
	bv[0].variance = box_variance(achv, &bv[0]);
	bv[0].sum = 0;
	bv[0].total_error = -1;
	for (uint i=0; i<bv[0].colors; i++) {
		bv[0].sum += achv[i].adjusted_weight;
	}
	
	uint boxes = 1;
	
	// remember smaller palette for fast searching
	colormap* representative_subset = NULL;
	uint subset_size = ceil(pow(newcolors,0.7));
	
	/*
	 ** Main loop: split boxes until we have enough.
	 */
	while (boxes < newcolors) {
		
		if (boxes == subset_size) {
			representative_subset = colormap_from_boxes(bv, boxes, achv, min_opaque_val);
		}
		
		int bi= best_splittable_box(bv, boxes);
		if (bi < 0)
			break;		  /* ran out of colors! */
		
		box& bx = bv[bi];
		uint indx = bx.ind;
		uint clrs = bx.colors;
		
		/*
		 Classic implementation tries to get even number of colors or pixels in each subdivision.

		 Here, instead of popularity I use (sqrt(popularity)*variance) metric.
		 Each subdivision balances number of pixels (popular colors) and low variance -
		 boxes can be large if they have similar colors. Later boxes with high variance
		 will be more likely to be split.

		 Median used as expected value gives much better results than mean.
		 */
		
		const double halfvar = prepare_sort(&bx, achv);
		double lowervar=0;
		
		// hist_item_sort_halfvar sorts and sums lowervar at the same time
		// returns item to break at ??minus one, which does smell like an off-by-one error.
		hist_item* break_p = hist_item_sort_halfvar(&achv[indx], clrs, &lowervar, halfvar);
		uint break_at = min<uint>(clrs-1, break_p - &achv[indx] + 1);
		
		/*
		 ** Split the box.
		 */
		double sm = bx.sum;
		double lowersum = 0;
		for (uint i=0; i<break_at; i++) {
			lowersum += achv[indx + i].adjusted_weight;
		}

		bx.colors = break_at;
		bx.sum = lowersum;
		bx.color = averagepixels(bx.colors, &achv[bx.ind], min_opaque_val);
		bx.total_error = -1;
		bx.variance = box_variance(achv, &bx);
		box& bx2 = bv[boxes];
		bx2.ind = indx + break_at;
		bx2.colors = clrs - break_at;
		bx2.sum = sm - lowersum;
		bx2.color = averagepixels(bx2.colors, &achv[bx2.ind], min_opaque_val);
		bx2.total_error = -1;
		bx2.variance = box_variance(achv, &bx2);
		
		++boxes;

		if (total_box_error_below_target(target_mse, bv, boxes, hist)) {
			break;
		}
	}

	colormap* map = colormap_from_boxes(bv, boxes, achv, min_opaque_val);
	map->subset_palette = representative_subset;
	adjust_histogram(achv, map, bv, boxes);
	delete bv;
	return map;
}

