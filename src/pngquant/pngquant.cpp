/* pngquant.c - quantize the colors in an alphamap down to a specified number
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
** Copyright (C) 1997, 2000, 2002 by Greg Roelofs; based on an idea by
**                                Stefan Schneider.
** ? 2009-2012 by Kornel Lesinski.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#define PNGQUANT_VERSION "1.8.0 (August 2012)"

#define PNGQUANT_USAGE "\
usage:	pngquant [options] [ncolors] [pngfile [pngfile ...]]\n\n\
options:\n\
  --force			overwrite existing output files (synonym: -f)\n\
  --nofs			disable Floyd-Steinberg dithering\n\
  --ext new.png		set custom suffix/extension for output filename\n\
  --speed N			speed/quality trade-off. 1=slow, 3=default, 10=fast & rough\n\
  --quality min-max don't save below min, use less colors below max (0-100)\n\
  --verbose			print status messages (synonym: -v)\n\
  --iebug			increase opacity to work around Internet Explorer 6 bug\n\
  --transbug		transparent color will be placed at the end of the palette\n\
\n\
Quantizes one or more 32-bit RGBA PNGs to 8-bit (or smaller) RGBA-palette\n\
PNGs using Floyd-Steinberg diffusion dithering (unless disabled).\n\
The output filename is the same as the input name except that\n\
it ends in \"-fs8.png\", \"-or8.png\" or your custom extension (unless the\n\
input is stdin, in which case the quantized image will go to stdout).\n\
The default behavior if the output file exists is to skip the conversion;\n\
use --force to overwrite.\n"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "getopt.h"
#include "dxor.h"

#if defined(WIN32) || defined(__WIN32__)
#  include <fcntl.h>	/* O_BINARY */
#  include <io.h>	/* setmode() */
#endif

#include "rwpng.h"	/* typedefs, common macros, public prototypes */
#include "pngquant.h"
#include "pam.h"
#include "mediancut.h"
#include "nearest.h"
#include "blur.h"
#include "viter.h"
#include <algorithm>

struct pngquant_options {
	double target_mse, max_mse;
	double min_opaque_val;
	uint reqcolors;
	uint speed_tradeoff;
	bool floyd, last_index_transparent;
};

static pngquant_error pngquant(png24_image* input_image, png8_image* output_image, const pngquant_options* options);
static pngquant_error read_image(struct rwpng_data *in_buffer, int using_stdin, png24_image* input_image_p);
static pngquant_error write_image(png8_image* output_image, png24_image* output_image24, struct rwpng_data *out_buffer, bool force, bool using_stdin);
static char* add_filename_extension(const char* filename, const char* newext);
static bool file_exists(const char* outname);

static bool verbose = 0;
/* prints only when verbose flag is set */
void verbose_printf(const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	if (verbose) vfprintf(stderr, fmt, va);
	va_end(va);
}

static
void print_full_version(FILE* fd)
{
	fprintf(fd, "pngquant, version %s, by Greg Roelofs, Kornel Lesinski.\n"
		#ifndef NDEBUG
					"	DEBUG (slow) version.\n"
		#endif
		#if USE_SSE
					"	Compiled with SSE2 instructions.\n"
		#endif
		#if _OPENMP
					"	Compiled with OpenMP (multicore support).\n"
		#endif
		, PNGQUANT_VERSION);
	rwpng_version_info(fd);
	fputs("\n", fd);
}

static
void print_usage(FILE* fd)
{
	fputs(PNGQUANT_USAGE, fd);
}

static
double quality_to_mse(long quality)
{
	if (quality == 0) return MAX_DIFF;

	// curve fudged to be roughly similar to quality of libjpeg
	return 1.1/pow(210.0 + quality, 1.2) * (100.1-quality)/100.0;
}

/**
 *	 N = automatic quality, uses limit unless force is set (N-N or 0-N)
 *	-N = no better than N (same as 0-N)
 * N-M = no worse than N, no better than M
 * N-  = no worse than N, perfect if possible (same as N-100)
 *
 * where N,M are numbers between 0 (lousy) and 100 (perfect)
 */
static
bool parse_quality(const char* quality, pngquant_options* options)
{
	long limit, target;
	const char* str = quality; char* end;
	
	long t1 = strtol(str, &end, 10);
	if (str == end) return false;
	str = end;

	if ('\0' == end[0] && t1 < 0) { // quality="-%d"
		target = -t1;
		limit = 0;
	}else if ('\0' == end[0]) { // quality="%d"
		target = t1;
		limit = t1*9/10;
	}else if ('-' == end[0] && '\0' == end[1]) { // quality="%d-"
		target = 100;
		limit = t1;
	}else { // quality="%d-%d"
		long t2 = strtol(str, &end, 10);
		if (str == end || t2 > 0) return false;
		target = -t2;
		limit = t1;
	}
	
	if (target < 0 || target > 100 || limit < 0 || limit > 100) return false;
	
	options->max_mse = quality_to_mse(limit);
	options->target_mse = quality_to_mse(target);
	return true;
}

static const struct {const char* old; char* newV;} obsolete_options[] = {
	{"-fs","--floyd"},
	{"-nofs", "--ordered"},
	{"-floyd", "--floyd"},
	{"-nofloyd", "--ordered"},
	{"-ordered", "--ordered"},
	{"-force", "--force"},
	{"-noforce", "--no-force"},
	{"-verbose", "--verbose"},
	{"-quiet", "--quiet"},
	{"-noverbose", "--quiet"},
	{"-noquiet", "--verbose"},
	{"-help", "--help"},
	{"-version", "--version"},
	{"-ext", "--ext"},
	{"-speed", "--speed"},
};

static
void fix_obsolete_options(const int argc, char* argv[])
{
	for (uint argn=1; argn<argc; argn++) {
		if ('-' != argv[argn][0]) continue;

		if ('-' == argv[argn][1]) break; // stop on first --option or --

		for (uint i=0; i<sizeof(obsolete_options)/sizeof(obsolete_options[0]); i++) {
			if (0 == strcmp(obsolete_options[i].old, argv[argn])) {
				fprintf(stderr, "  warning: option '%s' has been replaced with '%s'.\n", obsolete_options[i].old, obsolete_options[i].newV);
				argv[argn] = obsolete_options[i].newV;
			}
		}
	}
}

enum {arg_floyd=1, arg_ordered, arg_ext, arg_no_force, arg_iebug, arg_transbug, arg_quality};

static const option long_options[] = {
	{"verbose", no_argument, NULL, 'v'},
	{"quiet", no_argument, NULL, 'q'},
	{"force", no_argument, NULL, 'f'},
	{"no-force", no_argument, NULL, arg_no_force},
	{"floyd", no_argument, NULL, arg_floyd},
	{"ordered", no_argument, NULL, arg_ordered},
	{"nofs", no_argument, NULL, arg_ordered},
	{"iebug", no_argument, NULL, arg_iebug},
	{"transbug", no_argument, NULL, arg_transbug},
	{"ext", required_argument, NULL, arg_ext},
	{"speed", required_argument, NULL, 's'},
	{"quality", required_argument, NULL, arg_quality},
	{"version", no_argument, NULL, 'V'},
	{"help", no_argument, NULL, 'h'},
};

int pngquant_exec(struct rwpng_data * in_buffer, struct rwpng_data * out_buffer, int argc, char* argv[])
{
	pngquant_options options;
	options.reqcolors = 256;
	options.floyd = true; // floyd-steinberg dithering
	options.min_opaque_val = 1; // whether preserve opaque colors for IE (1.0=no, does not affect alpha)
	options.speed_tradeoff = 3; // 1 max quality, 10 rough & fast. 3 is optimum.
	options.last_index_transparent = false; // puts transparent color at last index. This is workaround for blu-ray subtitles.
	options.target_mse = 0;
	options.max_mse = MAX_DIFF;
	
	bool force = false, // force overwrite
		 using_stdin = false;
	uint latest_error=0, error_count=0, skipped_count=0, file_count=0;
	const char* filename;
	const char* newext = NULL;

	fix_obsolete_options(argc, argv);

	int opt;
	do {
		opt = getopt_long(argc, argv, "Vvqfhs:", long_options, NULL);
		switch (opt) {
		case 'v': verbose = true; break;
		case 'q': verbose = false; break;
		case arg_floyd: options.floyd = true; break;
		case arg_ordered: options.floyd = false; break;
		case 'f': force = true; break;
		case arg_no_force: force = false; break;
		case arg_ext: newext = optarg; break;

		case arg_iebug:
		options.min_opaque_val = 238.0/256.0; // opacities above 238 will be rounded up to 255, because IE6 truncates <255 to 0.
			break;
		case arg_transbug:
			options.last_index_transparent = true;
			break;

		case 's':
			options.speed_tradeoff = atoi(optarg);
			if (options.speed_tradeoff < 1 || options.speed_tradeoff > 10) {
				fputs("Speed should be between 1 (slow) and 10 (fast).\n", stderr);
				return INVALID_ARGUMENT;
			}
			break;

		case arg_quality:
			if (!parse_quality(optarg, &options)) {
				fputs("Quality should be in format min-max where min and max are numbers in range 0-100.\n", stderr);
				return INVALID_ARGUMENT;
			}
			break;

		case 'h':
			print_full_version(stdout);
			print_usage(stdout);
			return SUCCESS;

		case 'V':
			puts(PNGQUANT_VERSION);
			return SUCCESS;

		case -1: break;

		default:
			return INVALID_ARGUMENT;
		}
	} while (opt != -1);

	int argn = optind;

	if (argn >= argc) {
		if (argn > 1) {
			fputs("No input files specified. See -h for help.\n", stderr);
		} else {
			print_full_version(stderr);
			print_usage(stderr);
		}
		return MISSING_ARGUMENT;
	}

	char* colors_end;
	unsigned long colors = strtoul(argv[argn], &colors_end, 10);
	if (colors_end != argv[argn] && '\0' == colors_end[0]) {
		options.reqcolors = colors;
		argn++;
	}

	if (options.reqcolors < 2 || options.reqcolors > 256) {
		fputs("Number of colors must be between 2 and 256.\n", stderr);
		return INVALID_ARGUMENT;
	}

	// new filename extension depends on options used. Typically basename-fs8.png
	if (newext == NULL) {
		newext = options.floyd ? "-ie-fs8.png" : "-ie-or8.png";
		if (options.min_opaque_val == 1.f) newext += 3; /* skip "-ie" */
	}

	if (argn == argc || (argn == argc-1 && 0==strcmp(argv[argn],"-"))) {
		using_stdin = true;
		filename = "stdin";
		argn = argc;
	}else {
		filename = argv[argn];
		++argn;
	}

	/*=============================	 MAIN LOOP	=============================*/

	// while (argn <= argc) {
	do {
		int retval = 0;

		verbose_printf("%s:\n", filename);

		// char* outname = NULL;
		// if (!using_stdin) {
		// 	outname = add_filename_extension(filename,newext);
		// 	if (!force && file_exists(outname)) {
		// 		fprintf(stderr, "  error:  %s exists; not overwriting\n", outname);
		// 		retval = NOT_OVERWRITING_ERROR;
		// 	}
		// }

		png24_image input_image = {}; // initializes all fields to 0
		png8_image output_image = {};

		if (!retval) {
			retval = read_image(in_buffer,using_stdin,&input_image);
		}

		if (!retval) {
			verbose_printf("  read %uKB file corrected for gamma %2.1f\n", (input_image.file_size+1023)/1024,
						   1.0/input_image.gamma);

			retval = pngquant(&input_image, &output_image, &options);
		}

		if (!retval) {
			retval = write_image(&output_image, NULL, out_buffer, force, using_stdin);
			if (!using_stdin && retval) {
				out_buffer->png_data = in_buffer->png_data;
				out_buffer->length = in_buffer->length;
			}
		}else if (TOO_LOW_QUALITY == retval && using_stdin) {
			// when outputting to stdout it'd be nasty to create 0-byte file
			// so if quality is too low, output 24-bit original
			if (options.min_opaque_val == 1.0) {
				if (SUCCESS != write_image(NULL, &input_image, out_buffer, force, using_stdin)) {
					error_count++;
				}
			}else {
				// iebug preprocessing changes the original image
				fputs("	 error:	 can't write the original image when iebug option is enabled\n", stderr);
				error_count++;
			}
		}

		/* now we're done with the INPUT data and row_pointers, so free 'em */
		if (input_image.rgba_data) {
			free(input_image.rgba_data);
		}
		if (input_image.row_pointers) {
			free(input_image.row_pointers);
		}

//		if (outname) free(outname);

		if (output_image.indexed_data) {
			free(output_image.indexed_data);
		}
		if (output_image.row_pointers) {
			free(output_image.row_pointers);
		}

		if (retval) {
			latest_error = retval;
			if (retval == TOO_LOW_QUALITY) {
				skipped_count++;
			}else {
				error_count++;
			}
		}
		++file_count;

		verbose_printf("\n");

		filename = argv[argn];
		++argn;
	} while(0);

	/*=======================================================================*/


	if (error_count) {
		verbose_printf("There were errors quantizing %d file%s out of a total of %d file%s.\n",
					   error_count, (error_count == 1)? "" : "s", file_count, (file_count == 1)? "" : "s");
	}
	if (skipped_count) {
		verbose_printf("Skipped %d file%s out of a total of %d file%s.\n",
					   skipped_count, (skipped_count == 1)? "" : "s", file_count, (file_count == 1)? "" : "s");
	}
	if (!skipped_count && !error_count) {
		verbose_printf("No errors detected while quantizing %d image%s.\n",
					   file_count, (file_count == 1)? "" : "s");
	}

	return latest_error;
}

// ascend sort
static inline
bool compare_popularity(const colormap_item& v1, const colormap_item& v2)
{
	return v1.popularity < v2.popularity;
}

static
void sort_palette(
	png8_image* output_image,
	colormap* map,
	int last_index_transparent
	)
{
	assert(map); assert(output_image);
	
	/*
	** Step 3.5 [GRR]: remap the palette colors so that all entries with
	** the maximal alpha value (i.e., fully opaque) are at the end and can
	** therefore be omitted from the tRNS chunk.
	*/
	
	verbose_printf("  eliminating opaque tRNS-chunk entries...");
	output_image->num_palette = map->colors;
	
	if (last_index_transparent) for (uint i=0; i<map->colors; i++) {
		if (map->palette[i].acolor.alpha < 1.0/256.0) {
			const int old = i;
			const int transparent_dest = map->colors-1;
			const colormap_item tmp = map->palette[transparent_dest];
			map->palette[transparent_dest] = map->palette[old];
			map->palette[old] = tmp;

			/* colors sorted by popularity make pngs slightly more compressible */
			std::sort(map->palette, map->palette+map->colors, compare_popularity);

			output_image->num_trans = map->colors;
			return;
		}
	}

	/* move transparent colors to the beginning to shrink trns chunk */
	uint num_transparent = 0;
	for (uint i=0; i<map->colors; i++) {
		if (map->palette[i].acolor.alpha < 1.0) {
			// current transparent color is swapped with earlier opaque one
			if (i != num_transparent) {
				const colormap_item tmp = map->palette[num_transparent];
				map->palette[num_transparent] = map->palette[i];
				map->palette[i] = tmp;
				i--;
			}
			num_transparent++;
		}
	}

	verbose_printf("%d entr%s transparent\n", num_transparent, (num_transparent == 1)? "y" : "ies");

	/* colors sorted by popularity make pngs slightly more compressible
	 * opaque and transparent are sorted separately
	 */
	std::sort(map->palette, map->palette+num_transparent, compare_popularity);
	if (num_transparent < map->colors) {
		std::sort(map->palette+num_transparent, map->palette+num_transparent+(map->colors-num_transparent), compare_popularity);
	}

	output_image->num_trans = num_transparent;
}

static
void set_palette(png8_image* output_image, colormap* map)
{
	for (uint x=0; x<map->colors; ++x) {
		colormap_item& pal = map->palette[x];
		rgb_pixel px = to_rgb(output_image->gamma, pal.acolor);
		pal.acolor = to_f(output_image->gamma, px); /* saves rounding error introduced by to_rgb, which makes remapping & dithering more accurate */
//		rgb_pixel px = to_rgb(output_image->gamma, pal.acolor);
//		pal.acolor = to_f(output_image->gamma, px); /* saves rounding error introduced by to_rgb, which makes remapping & dithering more accurate */

		png_color& pc = output_image->palette[x];
		pc.red   = px.r;
		pc.green = px.g;
		pc.blue  = px.b;
		output_image->trans[x] = px.a;
	}
}

static
double remap_to_palette(
	png24_image* input_image,
	png8_image* output_image,
	colormap* const map,
	const double min_opaque_val
	)
{
	const rgb_pixel *const *const input_pixels = (const rgb_pixel **)input_image->row_pointers;
	unsigned char *const *const row_pointers = output_image->row_pointers;
	const int rows = input_image->height, cols = input_image->width;
	const double gamma = input_image->gamma;

	uint remapped_pixels = 0;
	double remapping_error=0;

	nearest_map* const n = nearest_init(map);
	f_pixel zp(0.0, 0.0, 0.0, 0.0);
	const int transparent_ind = nearest_search(n, zp, min_opaque_val, NULL);

	std::vector<viter_state> average_color(map->colors);
	viter_init(map, &average_color[0]);

	for (uint row=0; row<rows; ++row) {
		const rgb_pixel* const inputLine = input_pixels[row];
		unsigned char* const outputLine = row_pointers[row];
		for (uint col=0; col<cols; ++col) {

			f_pixel px = to_f(gamma, inputLine[col]);
			int match;

			if (px.alpha < 1.0/256.0) {
				match = transparent_ind;
			}else {
				double diff;
				match = nearest_search(n, px, min_opaque_val, &diff);

				remapped_pixels++;
				remapping_error += diff;
			}

			outputLine[col] = match;

			viter_update_color(px, 1.0, map, match, &average_color[0]);
		}
	}

	viter_finalize(map, &average_color[0]);

	nearest_free(n);

	return remapping_error / max(1u,remapped_pixels);
}

static
double distance_from_closest_other_color(const colormap *map, const int i)
{
	double second_best = MAX_DIFF;
	for (uint j=0; j<map->colors; j++) {
		if (i == j) continue;
		double diff = colordifference(map->palette[i].acolor, map->palette[j].acolor);
		second_best = min(diff, second_best);
	}
	return second_best;
}

/**
  Uses edge/noise map to apply dithering only to flat areas. Dithering on edges creates jagged lines, and noisy areas are "naturally" dithered.

  If output_image_is_remapped is true, only pixels noticeably changed by error diffusion will be written to output image.
 */
static
void remap_to_palette_floyd(
	png24_image *input_image,
	png8_image *output_image,
	const colormap *map,
	const double min_opaque_val,
	const double* edge_map,
	const int output_image_is_remapped
	)
{
	const rgb_pixel *const *const input_pixels = (const rgb_pixel *const *const)input_image->row_pointers;
	unsigned char *const *const row_pointers = output_image->row_pointers;
	const int rows = input_image->height, cols = input_image->width;
	const double gamma = input_image->gamma;

	const colormap_item* acolormap = map->palette;

	nearest_map*const n = nearest_init(map);
	f_pixel zp(0,0,0,0);
	const int transparent_ind = nearest_search(n, zp, min_opaque_val, NULL);

	std::vector<double> difference_tolerance(map->colors);
	if (output_image_is_remapped) for(uint i=0; i < map->colors; i++) {
		difference_tolerance[i] = distance_from_closest_other_color(map,i) / 4.0; // half of squared distance
	}

	/* Initialize Floyd-Steinberg error vectors. */
	f_pixel* RESTRICT thiserr = (f_pixel*) malloc((cols + 2) * sizeof(f_pixel));
	f_pixel* RESTRICT nexterr = (f_pixel*) malloc((cols + 2) * sizeof(f_pixel));
	sdxor156(12345); /* deterministic dithering is better for comparing results */
	const double INVFACTOR = 1.0 / 255.0;
	for (uint col=0; col<cols+2; ++col) {
		thiserr[col].r = (dxor156() - 0.5) * INVFACTOR;
		thiserr[col].g = (dxor156() - 0.5) * INVFACTOR;
		thiserr[col].b = (dxor156() - 0.5) * INVFACTOR;
		thiserr[col].alpha = (dxor156() - 0.5) * INVFACTOR;
	}
	
	bool fs_direction = true;
	for (uint row=0; row<rows; ++row) {
		const rgb_pixel* const inputLine = input_pixels[row];
		unsigned char* const rowLine = row_pointers[row];
		memset(nexterr, 0, (cols + 2) * sizeof(*nexterr));
		uint col = (fs_direction) ? 0 : (cols - 1);

		do {
			f_pixel px = to_f(gamma, inputLine[col]);

			double dither_level = edge_map ? edge_map[row*cols + col] : 0.9;

			/* Use Floyd-Steinberg errors to adjust actual color. */
			f_pixel tmp = px + thiserr[col + 1] * dither_level;
			// Error must be clamped, otherwise it can accumulate so much that it will be
			// impossible to compensate it, causing color streaks
			tmp.r = limitValue(tmp.r, 0.0, 1.0);
			tmp.g = limitValue(tmp.g, 0.0, 1.0);
			tmp.b = limitValue(tmp.b, 0.0, 1.0);
			tmp.alpha = limitValue(tmp.alpha, 0.0, 1.0);
			
			uint ind;
			if (tmp.alpha < 1.0/256.0) {
				ind = transparent_ind;
			}else {
				uint curr_ind = rowLine[col];
				if (output_image_is_remapped && colordifference(map->palette[curr_ind].acolor, tmp) < difference_tolerance[curr_ind]) {
					ind = curr_ind;
				}else {
					ind = nearest_search(n, tmp, min_opaque_val, NULL);
				}
			}

			rowLine[col] = ind;

			f_pixel err = tmp - acolormap[ind].acolor;
			
			// If dithering error is crazy high, don't propagate it that much
			// This prevents crazy geen pixels popping out of the blue (or red or black! ;)
			if (err.r*err.r + err.g*err.g + err.b*err.b + err.alpha*err.alpha > 16.0/256.0/256.0) {
				dither_level *= 0.75;
			}

			const double colorimp = (3.0 + acolormap[ind].acolor.alpha)/4.0 * dither_level;
			err.r *= colorimp;
			err.g *= colorimp;
			err.b *= colorimp;
			err.alpha *= dither_level;

			/* Propagate Floyd-Steinberg error terms. */
#if 1
			// changed kernel after reading the paper : Reinstating FloydSteinberg: Improved Metrics for Quality Assessment of Error Diffusion Algorithms (Sam Hocevar, Gary Niger)
			/* Propagate Floyd-Steinberg error terms. */
			if (fs_direction) {
				thiserr[col + 2] += err * 7.0 / 16.0;
				nexterr[col	   ] += err * 4.0 / 16.0;
				nexterr[col + 1] += err * 5.0 / 16.0;
				nexterr[col + 2] += err	* 0.0 / 16.0;
			}else {
				thiserr[col	   ] += err * 7.0 / 16.0;
				nexterr[col	   ] += err	* 0.0 / 16.0;
				nexterr[col + 1] += err * 5.0 / 16.0;
				nexterr[col + 2] += err * 4.0 / 16.0;
			}
#else
			if (fs_direction) {
				thiserr[col + 2] += err * 7.0/16.0;
				nexterr[col	   ] += err * 3.0/16.0;
				nexterr[col + 1] += err * 5.0/16.0;
				nexterr[col + 2] += err * 1.0/16.0;
			}else {
				thiserr[col	   ] += err * 7.0/16.0;
				nexterr[col	   ] += err * 1.0/16.0;
				nexterr[col + 1] += err * 5.0/16.0;
				nexterr[col + 2] += err * 3.0/16.0;
			}
#endif
			// remapping is done in zig-zag
			if (fs_direction) {
				++col;
				if (col >= cols) break;
			}else {
				if (col <= 0) break;
				--col;
			}
		}while(1);

		std::swap(thiserr, nexterr);
		fs_direction = !fs_direction;
	}

	free(thiserr);
	free(nexterr);
	nearest_free(n);
}

static bool file_exists(const char *outname)
{
	FILE* outfile = fopen(outname, "rb");
	if ((outfile ) != NULL) {
		fclose(outfile);
		return true;
	}
	return false;
}

/* build the output filename from the input name by inserting "-fs8" or
 * "-or8" before the ".png" extension (or by appending that plus ".png" if
 * there isn't any extension), then make sure it doesn't exist already */
static
char* add_filename_extension(const char* filename, const char* newext)
{
	size_t x = strlen(filename);

	char* outname = (char*) malloc(x+4+strlen(newext)+1);

	strncpy(outname, filename, x);
	if (strncmp(outname+x-4, ".png", 4) == 0)
		strcpy(outname+x-4, newext);
	else
		strcpy(outname+x, newext);

	return outname;
}

static
void set_binary_mode(FILE* fp)
{
#if defined(WIN32) || defined(__WIN32__)
	_setmode(fp == stdout ? 1 : 0, O_BINARY);
#endif
}

static
pngquant_error write_image(
	png8_image* output_image,
	png24_image* output_image24,
	struct rwpng_data *out_buffer,
	bool force,
	bool using_stdin
	)
{
	// FILE* outfile;
	// if (using_stdin) {
		// set_binary_mode(stdout);
		// outfile = stdout;

		// if (output_image) {
		// 	verbose_printf("  writing %d-color image to stdout\n", output_image->num_palette);
		// }else {
		// 	verbose_printf("  writing truecolor image to stdout\n");
		// }
	// }else {
		// if ((outfile = fopen(outname, "wb")) == NULL) {
		// 	fprintf(stderr, "  error:  cannot open %s for writing\n", outname);
		// 	return CANT_WRITE_ERROR;
		// }
		// const char* outfilename = strrchr(outname, '/');
		// if (outfilename) outfilename++; else outfilename = outname;

		// if (output_image) {
		// 	verbose_printf("  writing %d-color image as %s\n", output_image->num_palette, outfilename);
		// }else {
		// 	verbose_printf("  writing truecolor image as %s\n", outfilename);
		// }
	// }

	pngquant_error retval;
	if (output_image) {
		retval = rwpng_write_image8(out_buffer, output_image);
	}else {
		retval = rwpng_write_image24(out_buffer, output_image24);
	}

	// if (retval) {
	// 	fprintf(stderr, "  Error writing image to %s\n", outname);
	// }

	// if (!using_stdin)
	// 	fclose(outfile);

	/* now we're done with the OUTPUT data and row_pointers, too */
	return retval;
}

/* histogram contains information how many times each color is present in the image, weighted by importance_map */
static
histogram* get_histogram(
	const png24_image* input_image,
	const uint reqcolors,
	const uint speed_tradeoff,
	const double* importance_map
	)
{
	histogram* hist;
	uint ignorebits = 0;
	const rgb_pixel** input_pixels = (const rgb_pixel**) input_image->row_pointers;
	const uint cols = input_image->width, rows = input_image->height;
	const double gamma = input_image->gamma;
	assert(gamma > 0);

   /*
	** Step 2: attempt to make a histogram of the colors, unclustered.
	** If at first we don't succeed, increase ignorebits to increase color
	** coherence and try again.
	*/

	if (speed_tradeoff > 7) ignorebits++;
	uint maxcolors = (1<<17) + (1<<18)*(10-speed_tradeoff);

	verbose_printf("  making histogram...");
	for (;;) {
		hist = pam_computeacolorhist(input_pixels, cols, rows, gamma, maxcolors, ignorebits, importance_map);
		if (hist) break;
		
		ignorebits++;
		verbose_printf("too many colors!\n	scaling colors to improve clustering...");
	}
	
	verbose_printf("%d colors found\n", hist->size);
	return hist;
}

static
void modify_alpha(png24_image* input_image, const double min_opaque_val)
{
	/* IE6 makes colors with even slightest transparency completely transparent,
	   thus to improve situation in IE, make colors that are less than ~10% transparent
	   completely opaque */

	rgb_pixel *const *const input_pixels = (rgb_pixel **)input_image->row_pointers;
	const uint rows = input_image->height, cols = input_image->width;
	const double gamma = input_image->gamma;
	
	if (min_opaque_val > 254.0/255.0) return;
	
	const double almost_opaque_val = min_opaque_val * 169.0/256.0;
	const uint almost_opaque_val_int = almost_opaque_val*255.0;
	
	verbose_printf("  Working around IE6 bug by making image less transparent...\n");
	
	for (uint row=0; row<rows; ++row) {
		for (uint col=0; col<cols; col++) {
			const rgb_pixel srcpx = input_pixels[row][col];

			/* ie bug: to avoid visible step caused by forced opaqueness, linearily raise opaqueness of almost-opaque colors */
			if (srcpx.a >= almost_opaque_val_int) {
				f_pixel px = to_f(gamma, srcpx);

				double al = almost_opaque_val + (px.alpha-almost_opaque_val) * (1-almost_opaque_val) / (min_opaque_val-almost_opaque_val);
				if (al > 1) al = 1;
				px.alpha = al;
				input_pixels[row][col] = to_rgb(gamma, px);
			}
		}
	}
}

static
pngquant_error read_image(struct rwpng_data * in_buffer, int using_stdin, png24_image* input_image_p)
{
	// FILE* infile;
	
	// if (using_stdin) {
	// 	set_binary_mode(stdin);
	// 	infile = stdin;
	// }else if ((infile = fopen(filename, "rb")) == NULL) {
	// 	fprintf(stderr, "  error:  cannot open %s for reading\n", filename);
	// 	return READ_ERROR;
	// }
	
	/*
	 ** Step 1: read in the alpha-channel image.
	 */
	/* GRR:	 returns RGBA (4 channels), 8 bps */
	pngquant_error retval = rwpng_read_image24(in_buffer, input_image_p);
	
	// if (!using_stdin) {
	// 	fclose(infile);
	// }

	if (retval) {
		fprintf(stderr, "  rwpng_read_image() error\n");
		return retval;
	}

	return SUCCESS;
}

/**
 Builds two maps:
	noise - approximation of areas with high-frequency noise, except straight edges. 1=flat, 0=noisy.
	edges - noise map including all edges
 */
static
void contrast_maps(
	const f_pixel* pixels,
	size_t cols,
	size_t rows,
	double* noise,
	double* edges
	)
{
	std::vector<double> tmpVec(cols*rows);
	double* tmp = &tmpVec[0];
	
	const f_pixel* pSrc = pixels;
	double* pNoise = noise;
	double* pEdges = edges;
	for (size_t y=0; y<rows; ++y) {
		f_pixel prev, curr = pSrc[0], next=curr;
		const f_pixel* nextLine = (y == 0) ? pSrc : (pSrc - cols);
		const f_pixel* prevLine = (y == rows-1) ? pSrc : (pSrc + cols);
		for (size_t x=0; x<cols; ++x) {
			prev = curr;
			curr = next;
			next = pSrc[min(cols-1,x+1)];
			// contrast is difference between pixels neighbouring horizontally and vertically
			f_pixel hd = (prev + next - curr * 2.0).abs();
			f_pixel nextl = nextLine[x];
			f_pixel prevl = prevLine[x];
			f_pixel vd = (prevl + nextl - curr * 2.0).abs();
			double horiz = max(hd.alpha, hd.r, hd.g, hd.b);
			double vert = max(vd.alpha, vd.r, vd.g, vd.b);
			double edge = max(horiz, vert);
			double z = edge - fabs(horiz-vert)*.5;
			z = 1.0 - max(z,min(horiz,vert));
			z *= z; // noise is amplified
			z *= z;

			pNoise[x] = z;
			pEdges[x] = 1.0 - edge;
		}
		pSrc += cols;
		pNoise += cols;
		pEdges += cols;
	}

	// noise areas are shrunk and then expanded to remove thin edges from the map
	max3(noise, tmp, cols, rows);
	max3(tmp, noise, cols, rows);

	blur(noise, tmp, noise, cols, rows, 3);

	max3(noise, tmp, cols, rows);

	min3(tmp, noise, cols, rows);
	min3(noise, tmp, cols, rows);
	min3(tmp, noise, cols, rows);

	min3(edges, tmp, cols, rows);
	max3(tmp, edges, cols, rows);
	for (uint i=0; i<cols*rows; i++) {
		edges[i] = min(noise[i], edges[i]);
	}
}

/**
 * Builds map of neighbor pixels mapped to the same palette entry
 *
 * For efficiency/simplicity it mainly looks for same consecutive pixels horizontally
 * and peeks 1 pixel above/below. Full 2d algorithm doesn't improve it significantly.
 * Correct flood fill doesn't have visually good properties.
 */
static
void update_dither_map(const png8_image* output_image, double* edges)
{
	const uint width = output_image->width;
	const uint height = output_image->height;
	const unsigned char* RESTRICT pixels = (const unsigned char*) output_image->indexed_data;

	for (uint row=0; row<height; row++) {
		unsigned char lastpixel = pixels[row*width];
		uint lastcol = 0;
		double* pEdge = &edges[row*width];
		const unsigned char* pPrevLine = &pixels[(row-1)*width];
		const unsigned char* pNextLine = &pixels[(row+1)*width];
		for (uint col=1; col<width; col++) {
			const unsigned char px = pixels[row*width + col];

			if (px != lastpixel || col == width-1) {
				double neighbor_count = 2.5 + col-lastcol;

				uint i=lastcol;
				while (i < col) {
					if (row > 0) {
						if (pPrevLine[i] == lastpixel) neighbor_count += 1.0;
					}
					if (row < height-1) {
						if (pNextLine[i] == lastpixel) neighbor_count += 1.0;
					}
					i++;
				}
				while (lastcol <= col) {
					pEdge[lastcol++] *= 1.0 - 2.50/neighbor_count;
				}
				lastpixel = px;
			}
		}
	}
}

static
void adjust_histogram_callback(hist_item* item, double diff)
{
	item->adjusted_weight = (item->perceptual_weight+item->adjusted_weight) * (sqrtf(1.0+diff));
}

void convert(const rgb_pixel*const apixels[], size_t cols, size_t rows, double gamma, f_pixel* dest)
{
	f_pixel* pDst = dest;
	for (size_t y=0; y<rows; ++y) {
		const rgb_pixel* pSrc = apixels[y];
		for (size_t x=0; x<cols; ++x) {
			f_pixel lab = to_f(gamma, pSrc[x]);
			pDst[x] = lab;
//			pDst[x] = to_f(gamma, pSrc[x]);
		}
		pDst += cols;
	}
}

/**
 Repeats mediancut with different histogram weights to find palette with minimum error.

 feedback_loop_trials controls how long the search will take. < 0 skips the iteration.
 */
static
colormap* find_best_palette(
	histogram* hist,
	uint reqcolors,
	const double min_opaque_val,
	const double target_mse,
	int feedback_loop_trials,
	double* palette_error_p
	)
{
	colormap* acolormap = NULL;
	double least_error = 0.0;
	double target_mse_overshoot = feedback_loop_trials>0 ? 1.05 : 1.0;
	const double percent = (double)(feedback_loop_trials>0?feedback_loop_trials:1)/100.0;

	do {
		verbose_printf("  selecting colors");

		colormap* newmap = mediancut(hist, min_opaque_val, reqcolors, target_mse * target_mse_overshoot);
		if (newmap->subset_palette) {
			// nearest_search() needs subset palette to accelerate the search, I presume that
			// palette starting with most popular colors will improve search speed
			std::sort(newmap->subset_palette->palette, newmap->subset_palette->palette+newmap->subset_palette->colors, compare_popularity);
		}

		if (feedback_loop_trials <= 0) {
			verbose_printf("\n");
			return newmap;
		}

		verbose_printf("...");

		// after palette has been created, total error (MSE) is calculated to keep the best palette
		// at the same time Voronoi iteration is done to improve the palette
		// and histogram weights are adjusted based on remapping error to give more weight to poorly matched colors

		const bool first_run_of_target_mse = !acolormap && target_mse > 0.0;
		double total_error = viter_do_iteration(hist, newmap, min_opaque_val, first_run_of_target_mse ? NULL : adjust_histogram_callback);

		// goal is to increase quality or to reduce number of colors used if quality is good enough
		if (!acolormap || total_error < least_error || (total_error <= target_mse && newmap->colors < reqcolors)) {
			if (acolormap) pam_freecolormap(acolormap);
			acolormap = newmap;

			if (total_error < target_mse && total_error > 0.0) {
				// voronoi iteration improves quality above what mediancut aims for
				// this compensates for it, making mediancut aim for worse
				target_mse_overshoot = min(target_mse_overshoot*1.25, target_mse/total_error);
			}

			least_error = total_error;

			// if number of colors could be reduced, try to keep it that way
			// but allow extra color as a bit of wiggle room in case quality can be improved too
			reqcolors = min<uint>(newmap->colors+1, reqcolors);

			feedback_loop_trials -= 1; // asymptotic improvement could make it go on forever
		}else {
			target_mse_overshoot = 1.0;
			feedback_loop_trials -= 6;
			// if error is really bad, it's unlikely to improve, so end sooner
			if (total_error > least_error*4) feedback_loop_trials -= 3;
			pam_freecolormap(newmap);
		}
		verbose_printf("%d%%\n",100-max(0,(int)(feedback_loop_trials/percent)));
	}while (feedback_loop_trials > 0);

	*palette_error_p = least_error;
	return acolormap;
}

static
pngquant_error pngquant(
	png24_image* input_image,
	png8_image* output_image,
	const pngquant_options* options
	)
{
	const uint speed_tradeoff = options->speed_tradeoff, reqcolors = options->reqcolors;
	const double max_mse = options->max_mse, target_mse = options->target_mse;
	const double min_opaque_val = options->min_opaque_val;
	assert(min_opaque_val>0.0);
	assert(max_mse >= target_mse);
	
	modify_alpha(input_image, min_opaque_val);
	size_t width = input_image->width;
	size_t height = input_image->height;
	std::vector<f_pixel> input(width * height);
	convert(
		(const rgb_pixel**)input_image->row_pointers,
		width, height,
		input_image->gamma, &input[0]
		);
	
	std::vector<double> noise(width * height);
	std::vector<double> edges(width * height);
	if (speed_tradeoff < 8 && width >= 4 && height >= 4) {
		contrast_maps(
			&input[0],
			width, height,
			&noise[0], &edges[0]
			);
	}
	
	// histogram uses noise contrast map for importance. Color accuracy in noisy areas is not very important.
	// noise map does not include edges to avoid ruining anti-aliasing
	histogram* hist = get_histogram(input_image, reqcolors, speed_tradeoff, &noise[0]);
	
	for (uint i=0; i<hist->size; ++i) {
		hist_item& item = hist->achv[i];
		item.acolor = item.acolor;
	}
	
	double palette_error = -1;
	colormap* acolormap = find_best_palette(hist, reqcolors, min_opaque_val, target_mse, 56-9*speed_tradeoff, &palette_error);

	// Voronoi iteration approaches local minimum for the palette
	uint iterations = max(8u-speed_tradeoff,0u); iterations += iterations * iterations/2;
	if (!iterations && palette_error < 0 && max_mse < MAX_DIFF) iterations = 1; // otherwise total error is never calculated and MSE limit won't work

	if (iterations) {
		verbose_printf("  moving colormap towards local minimum\n");

		const double iteration_limit = 1.0/(double)(1<<(23-speed_tradeoff));
		double previous_palette_error = MAX_DIFF;
		for (uint i=0; i<iterations; i++) {
			palette_error = viter_do_iteration(hist, acolormap, min_opaque_val, NULL);
			
			if (abs(previous_palette_error-palette_error) < iteration_limit) {
				break;
			}
			
			if (palette_error > max_mse*1.5) { // probably hopeless
				if (palette_error > max_mse*3.0) break; // definitely hopeless
				iterations++;
			}
			
			previous_palette_error = palette_error;
		}
	}
	
	pam_freeacolorhist(hist);

	if (palette_error > max_mse) {
		verbose_printf("  image degradation MSE=%.3f exceeded limit of %.3f\n", palette_error*65536.0, max_mse*65536.0);
		pam_freecolormap(acolormap);
		return TOO_LOW_QUALITY;
	}

	output_image->width = input_image->width;
	output_image->height = input_image->height;
	output_image->gamma = SRGB_GAMMA; // fixed gamma ~2.2 for the web. PNG can't store exact 1/2.2

	/*
	** Step 3.7 [GRR]: allocate memory for the entire indexed image
	*/

	output_image->indexed_data = (unsigned char*) malloc(output_image->height * output_image->width);
	output_image->row_pointers = (unsigned char**) malloc(output_image->height * sizeof(output_image->row_pointers[0]));

	if (!output_image->indexed_data || !output_image->row_pointers) {
		return OUT_OF_MEMORY_ERROR;
	}

	for (uint row = 0; row<output_image->height; ++row) {
		output_image->row_pointers[row] = output_image->indexed_data + row*output_image->width;
	}

	// tRNS, etc.
	sort_palette(output_image, acolormap, options->last_index_transparent);

	/*
	 ** Step 4: map the colors in the image to their closest match in the
	 ** new colormap, and write 'em out.
	 */
	verbose_printf("  mapping image to new colors...");

	const int floyd = options->floyd,
			  use_dither_map = floyd && speed_tradeoff < 6;

	if (!floyd || use_dither_map) {
		// If no dithering is required, that's the final remapping.
		// If dithering (with dither map) is required, this image is used to find areas that require dithering
		double remapping_error = remap_to_palette(input_image, output_image, acolormap, min_opaque_val);

		// remapping error from dithered image is absurd, so always non-dithered value is used
		// palette_error includes some perceptual weighting from histogram which is closer correlated with dssim
		// so that should be used when possible.
		if (palette_error < 0) {
			palette_error = remapping_error;
		}

		if (use_dither_map) {
			update_dither_map(output_image, &edges[0]);
		}
	}

	if (palette_error >= 0) {
		verbose_printf("MSE=%.3f", palette_error*65536.0);
	}

	// remapping above was the last chance to do voronoi iteration, hence the final palette is set after remapping
	set_palette(output_image, acolormap);

	if (floyd) {
		remap_to_palette_floyd(input_image, output_image, acolormap, min_opaque_val, &edges[0], use_dither_map);
	}

	verbose_printf("\n");

	pam_freecolormap(acolormap);

	return SUCCESS;
}


