
#include "pam.h"
#include "nearest.h"
#include "mempool.h"
#include <stdlib.h>
#include <algorithm>

struct color_entry {
	f_pixel color;
	double radius;
	uint index;
};

struct sorttmp {
	double radius;
	uint index;
};

struct head {
	f_pixel center;
	double radius;
	uint num_candidates;
	color_entry* candidates;
};

struct nearest_map {
	head* heads;
	mempool* mempool;
	uint num_heads;
};

// ascend sort
static
bool compareradius(const sorttmp& ap, const sorttmp& bp)
{
	return ap.radius < bp.radius;
}

head build_head(
	f_pixel px,
	const colormap* map,
	int num_candidates,
	mempool* m,
	uint skip_index[],
	uint *skipped
	)
{
	std::vector<sorttmp> colors(map->colors);
	uint colorsused=0;

	for (uint i=0; i<map->colors; i++) {
		if (skip_index[i]) continue;
		colors[colorsused].index = i;
		colors[colorsused].radius = colordifference(px, map->palette[i].acolor);
		colorsused++;
	}
	
	std::sort(&colors[0], &colors[0]+colorsused, compareradius);
	assert(colorsused < 2 || colors[0].radius <= colors[1].radius);
	
	num_candidates = min<int>(colorsused, num_candidates);
	
	head h;
	h.candidates = (color_entry*) mempool_new(m, num_candidates * sizeof(h.candidates[0]));
	h.center = px;
	h.num_candidates = num_candidates;
	for (uint i=0; i<num_candidates; ++i) {
		color_entry entry;
		entry.color = map->palette[colors[i].index].acolor;
		entry.index = colors[i].index;
		entry.radius = colors[i].radius;
		h.candidates[i] = entry;
	}
	h.radius = colors[num_candidates-1].radius/4.0; // /2 squared
	
	for (uint i=0; i<num_candidates; ++i) {
		assert(colors[i].radius <= h.radius*4.0);
		// divide again as that's matching certain subset within radius-limited subset
		// - 1/256 is a tolerance for miscalculation (seems like colordifference isn't exact)
		if (colors[i].radius < h.radius/4.0 - 1.0/256.0) {
			skip_index[colors[i].index]=1;
			(*skipped)++;
		}
	}
	return h;
}

static
colormap* get_subset_palette(const colormap* map)
{
	// it may happen that it gets palette without subset palette or the subset is too large
	int subset_size = (map->colors+3)/4;
	
	if (map->subset_palette && map->subset_palette->colors <= subset_size) {
		return map->subset_palette;
	}
	
	const colormap* source = map->subset_palette ? map->subset_palette : map;
	colormap* subset_palette = pam_colormap(subset_size);
	
	for (uint i=0; i<subset_size; i++) {
		subset_palette->palette[i] = source->palette[i];
	}
	
	return subset_palette;
}


nearest_map* nearest_init(const colormap* map)
{
	mempool* m = NULL;
	nearest_map* centroids = (nearest_map*) mempool_new(m, sizeof(*centroids));
	centroids->mempool = m;
	
	uint skipped=0;
	std::vector<uint> skip_index(map->colors);
	
	colormap* subset_palette = get_subset_palette(map);
	const int selected_heads = subset_palette->colors;
	centroids->heads = (head*) mempool_new(centroids->mempool, sizeof(centroids->heads[0])*(selected_heads+1)); // +1 is fallback head

	uint h=0;
	for (; h < selected_heads; h++) {
		uint num_candiadtes = 1+(map->colors - skipped)/((1+selected_heads-h)/2);
		
		centroids->heads[h] = build_head(subset_palette->palette[h].acolor, map, num_candiadtes, centroids->mempool, &skip_index[0], &skipped);
		if (centroids->heads[h].num_candidates == 0) {
			break;
		}
	}

	centroids->heads[h].radius = MAX_DIFF;
	f_pixel px; px.alpha = px.r = px.g = px.b = 0.0;
	centroids->heads[h].center = px;
	centroids->heads[h].num_candidates = 0;
	centroids->heads[h].candidates = (color_entry*) mempool_new(centroids->mempool, (map->colors - skipped) * sizeof(centroids->heads[h].candidates[0]));
	for (uint i=0; i<map->colors; i++) {
		if (skip_index[i]) continue;
		color_entry entry;
		entry.color = map->palette[i].acolor;
		entry.index = i;
		entry.radius = 999.0;
		centroids->heads[h].candidates[centroids->heads[h].num_candidates++] = entry;
	}
	centroids->num_heads = ++h;
	
	// get_subset_palette could have created a copy
	if (subset_palette != map->subset_palette) {
		pam_freecolormap(subset_palette);
	}

	return centroids;
}

uint nearest_search(const nearest_map* centroids, const f_pixel px, const double min_opaque_val, double* diff)
{
	const int iebug = px.alpha > min_opaque_val;

	const head* const heads = centroids->heads;
	for (uint i=0; i<centroids->num_heads; i++) {
		double headdist = colordifference(px, heads[i].center);

		if (headdist <= heads[i].radius) {
			assert(heads[i].num_candidates);
			uint ind=heads[i].candidates[0].index;
			double dist = colordifference(px, heads[i].candidates[0].color);

			/* penalty for making holes in IE */
			if (iebug && heads[i].candidates[0].color.alpha < 1.0) {
				dist += 1.0/1024.0;
			}

			for (uint j=1; j<heads[i].num_candidates; j++) {
				double newdist = colordifference(px, heads[i].candidates[j].color);

				/* penalty for making holes in IE */
				if (iebug && heads[i].candidates[j].color.alpha < 1.0) {
					newdist += 1.0/1024.0;
				}

				if (newdist < dist) {
					dist = newdist;
					ind = heads[i].candidates[j].index;
				}
			}
			if (diff) *diff = dist;
			return ind;
		}
	}
	assert(0);
	return 0;
}

void nearest_free(nearest_map* centroids)
{
	mempool_free(centroids->mempool);
}

