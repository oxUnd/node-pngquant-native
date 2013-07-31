#pragma once

/**
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

#include <vector>
#include <math.h>
#include <assert.h>
#include "common.h"
#include "mempool.h"
#include "rwpng.h"

#define MAX_DIFF 1e20

template <typename T>
T min(T a, T b)
{
	if (a < b) {
		return a;
	}else {
		return b;
	}
}

template <typename T>
T min(T a, T b, T c)
{
	return min(min(a,b), c);
}

template <typename T>
T min(T a, T b, T c, T d)
{
	return min(min(a,b), min(c, d));
}

template <typename T>
T min(T a, T b, T c, T d, T e)
{
	return min(min(a,b,c,d), e);
}

template <typename T>
T max(T a, T b)
{
	if (a < b) {
		return b;
	}else {
		return a;
	}
}

template <typename T>
T max(T a, T b, T c)
{
	return max(max(a,b), c);
}

template <typename T>
T max(T a, T b, T c, T d)
{
	return max(max(a, b), max(c, d));
}

template <typename T>
T max(T a, T b, T c, T d, T e)
{
	return max(max(a, b, c, d), e);
}

template <typename T>
T limitValue(T val, T min, T max)
{
	if (val < min) return min;
	if (max < val) return max;
	return val;
}

/* from pam.h */

struct rgb_pixel {
/*
	rgb_pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
		:
		r(r),
		g(g),
		b(b),
		a(a)
	{
	}
*/
	unsigned char r, g, b, a;

	rgb_pixel& operator *= (int v)
	{
		r *= v;
		g *= v;
		b *= v;
		a *= v;
		return *this;
	}

	rgb_pixel& operator /= (int v)
	{
		r /= v;
		g /= v;
		b /= v;
		a /= v;
		return *this;
	}
};

static inline
rgb_pixel operator * (const rgb_pixel& l, int v)
{
	rgb_pixel ret = l;
	ret *= v;
	return ret;
}

static inline
rgb_pixel operator / (const rgb_pixel& l, int v)
{
	rgb_pixel ret = l;
	ret /= v;
	return ret;
}



struct f_pixel {
	f_pixel(double alpha, double r, double g, double b)
		:
		alpha(alpha),
		r(r),
		g(g),
		b(b)
	{
	}
	
	f_pixel& operator += (const f_pixel& p)
	{
		alpha += p.alpha;
		r += p.r;
		g += p.g;
		b += p.b;
		return *this;
	}

	f_pixel& operator - ()
	{
		alpha = -alpha;
		r = -r;
		g = -g;
		b = -b;
		return *this;
	}

	f_pixel& operator -= (const f_pixel& p)
	{
		alpha -= p.alpha;
		r -= p.r;
		g -= p.g;
		b -= p.b;
		return *this;
	}

	f_pixel& operator *= (double v)
	{
		alpha *= v;
		r *= v;
		g *= v;
		b *= v;
		return *this;
	}

	f_pixel& operator *= (const f_pixel& p)
	{
		alpha *= p.alpha;
		r *= p.r;
		g *= p.g;
		b *= p.b;
		return *this;
	}

	f_pixel& operator /= (double v)
	{
		alpha /= v;
		r /= v;
		g /= v;
		b /= v;
		return *this;
	}

	f_pixel& square()
	{
		alpha *= alpha;
		r *= r;
		g *= g;
		b *= b;
		return *this;
	}

	f_pixel& abs()
	{
		alpha = ::abs(alpha);
		r = ::abs(r);
		g = ::abs(g);
		b = ::abs(b);
		return *this;
	}

	f_pixel() {}
	double alpha;
	
	union {
		struct { double r, g, b; };
		struct { double x, y, z; };
		struct { double l, a, b; };
		double arr[3];
	};
};

static inline
f_pixel operator + (const f_pixel& l, const f_pixel& r)
{
	f_pixel ret = l;
	ret += r;
	return ret;
}

static inline
f_pixel operator - (const f_pixel& l, const f_pixel& r)
{
	f_pixel ret = l;
	ret -= r;
	return ret;
}

static inline
f_pixel max(const f_pixel& l, const f_pixel& r)
{
	return f_pixel(
		max(l.alpha, r.alpha),
		max(l.r, r.r),
		max(l.g, r.g),
		max(l.b, r.b)
		);
}

static inline
f_pixel operator * (const f_pixel& l, double v)
{
	f_pixel ret = l;
	ret *= v;
	return ret;
}

static inline
f_pixel operator / (const f_pixel& l, double v)
{
	f_pixel ret = l;
	ret /= v;
	return ret;
}

static inline
f_pixel operator / (const rgb_pixel& l, double v)
{
	f_pixel ret(l.a, l.r, l.g, l.b);
	ret /= v;
	return ret;
}

static inline
bool operator == (const f_pixel& l, const f_pixel& r)
{
	return 1
		&& l.alpha == r.alpha
		&& l.r == r.r
		&& l.g == r.g
		&& l.b == r.b
		;
}

// http://www.cgsd.com/papers/gamma_colorspace.html
static const double INTERNAL_GAMMA = SRGB_GAMMA;	// adjust this value depends on image hehehe

/**
 Converts 8-bit color to internal gamma and premultiplied alpha.
 (premultiplied color space is much better for blending of semitransparent colors)
 */
static inline
f_pixel to_f_scalar(double gamma, f_pixel px)
{
	if (gamma != INTERNAL_GAMMA) {
		px.r = pow(px.r, INTERNAL_GAMMA/gamma);
		px.g = pow(px.g, INTERNAL_GAMMA/gamma);
		px.b = pow(px.b, INTERNAL_GAMMA/gamma);
	}

	px.r *= px.alpha;
	px.g *= px.alpha;
	px.b *= px.alpha;

	return px;
}

/**
  Converts 8-bit RGB with given gamma to scalar RGB
 */
static inline
f_pixel to_f(double gamma, rgb_pixel px)
{
	return to_f_scalar(
		gamma,
		px / 255.0
	);
}

static inline
rgb_pixel to_rgb(double gamma, f_pixel px)
{
	if (px.alpha < 1.0/256.0) {
		rgb_pixel ret;
		ret.r = ret.g = ret.b = ret.a = 0;
		return ret;
	}

    double r = px.r / px.alpha,
          g = px.g / px.alpha,
          b = px.b / px.alpha,
          a = px.alpha;

    if (gamma != INTERNAL_GAMMA) {
        r = pow(r, gamma/INTERNAL_GAMMA);
        g = pow(g, gamma/INTERNAL_GAMMA);
        b = pow(b, gamma/INTERNAL_GAMMA);
    }

    // 256, because numbers are in range 1..255.9999… rounded down
    r *= 256.0;
    g *= 256.0;
    b *= 256.0;
    a *= 256.0;

	rgb_pixel ret;
	ret.r = r>=255.0 ? 255 : (r<=0.0 ? 0 : r);
	ret.g = g>=255.0 ? 255 : (g<=0.0 ? 0 : g);
	ret.b = b>=255.0 ? 255 : (b<=0.0 ? 0 : b);
	ret.a = a>=255.0 ? 255 : a;
	return ret;
}

#if 1

static inline
f_pixel rgb2xyz(f_pixel rgb)
{
	f_pixel xyz;
	xyz.x = 0.4124564 * rgb.r + 0.3575761 * rgb.g + 0.1804375 * rgb.b; // X
	xyz.y = 0.2126729 * rgb.r + 0.7151522 * rgb.g + 0.0721750 * rgb.b; // Y
	xyz.z = 0.0193339 * rgb.r + 0.1191920 * rgb.g + 0.9503041 * rgb.b; // Z
	xyz.alpha = rgb.alpha;
	return xyz;
}

static inline
f_pixel xyz2rgb(f_pixel xyz)
{
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_RGB.html
	f_pixel rgb;
	rgb.r = 3.2404542 * xyz.x -1.5371385 * xyz.y -0.4985314 * xyz.z; // X
	rgb.g = -0.9692660 * xyz.x + 1.8760108 * xyz.y + 0.0415560 * xyz.z; // Y
	rgb.b = 0.0556434 * xyz.x -0.2040259 * xyz.y + 1.0572252 * xyz.z; // Z
	rgb.alpha = xyz.alpha;
	return rgb;
}

static inline
f_pixel lab2xyz(f_pixel lab)
{
	lab.l *= 100.0;
	lab.a *= 354.0;
	lab.b *= 262.0;
	lab.a -= 134.0;
	lab.b -= 140.0;

	// http://www.brucelindbloom.com/index.html?Eqn_Lab_to_XYZ.html
	static const double E = 216.0 / 24389.0;
	static const double K = 24389.0 / 27.0;
	static const double KE = K * E;
	f_pixel f;
	f.y = (lab.l + 16.0) / 116.0;
	f.x = lab.a / 500.0 + f.y;
	f.z = f.y - lab.b / 200.0;
	
	f_pixel f3 = f;
	f3 *= f;
	f3 *= f;
	f_pixel xyz;
#if 1
	if (f3.x > E) {
		xyz.x = f3.x;
	}else {
		xyz.x = ((f.x * 116) - 16) / K;
	}
	if (f3.y > E) {
		xyz.y = f3.y;
	}else {
		xyz.y = ((f.y * 116) - 16) / K;
	}
	if (f3.z > E) {
		xyz.z = f3.z;
	}else {
		xyz.z = ((f.z * 116) - 16) / K;
	}
#else
	if (f3.x > E) {
		xyz.x = f3.x;
	}else {
		xyz.x = (116.0 * f.x - 16.0) / K;
	}
	if (lab.l > KE) {
		double t = (lab.l + 16.0) / 116.0;
		xyz.y = t * t * t;
	}else {
		xyz.y = lab.l / K;
	}
	if (f3.z > E) {
		xyz.z = f3.z;
	}else {
		xyz.z = (116.0 * f.z - 16.0) / K;
	}
#endif
	xyz.alpha = lab.alpha;
	return xyz;
}

static inline
f_pixel xyz2lab(f_pixel xyz)
{
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_Lab.html
	static const double K = 24389.0 / 27.0;
	static const double S = 216.0 / 24389.0;
	f_pixel f;
	f_pixel lab;
	// fx
	if (xyz.x <= S) {
		f.x = (K * xyz.x + 16) / 116;
	}else {
		f.x = pow(xyz.x, 1.0/3.0);
	}
	// fy
	if (xyz.y <= S) {
		f.y = (K * xyz.y + 16) / 116;
	}else {
		f.y = pow(xyz.y, 1.0/3.0);
	}
	// fz
	if (xyz.z <= S) {
		f.z = (K * xyz.z + 16) / 116;
	}else {
		f.z = pow(xyz.z, 1.0/3.0);
	}
	lab.l = 116 * f.y -16;
	lab.a = 500 * (f.x - f.y);
	lab.b = 200 * (f.y - f.z);
	lab.alpha = xyz.alpha;

	// normalize
	lab.l /= 100.0;
	lab.a = (lab.a + 134) / 354.0;
	lab.b = (lab.b + 140) / 262.0;

	return lab;
}

static inline
f_pixel rgb2lab(f_pixel rgb)
{
	return xyz2lab(rgb2xyz(rgb));
}

static inline
f_pixel lab2rgb(f_pixel lab)
{
	return xyz2rgb(lab2xyz(lab));
}

#endif

inline static
double colordifference_ch(const double x, const double y, const double alphas)
{
    // maximum of channel blended on white, and blended on black
    // premultiplied alpha and backgrounds 0/1 shorten the formula
    const double black = x-y, white = black+alphas;
    return max(black*black, white*white);
}

static inline
double colordifference_stdc(const f_pixel px, const f_pixel py)
{
    const double alphas = py.alpha - px.alpha;
    return colordifference_ch(px.r, py.r, alphas) +
           colordifference_ch(px.g, py.g, alphas) +
           colordifference_ch(px.b, py.b, alphas);
}

static inline
double colordifference(f_pixel px, f_pixel py)
{
#if 0
    return colordifference_stdc(px,py);
#else
	
	f_pixel diff = px - py;
#if 1
	f_pixel tmp = diff;
	diff *= tmp;
	diff *= tmp;
	diff.abs();
#else
	diff.square();
#endif
	return
		diff.alpha * 2
		+ diff.l * 1.0
		+ diff.a * 1.0
		+ diff.b * 1.0
		;
#endif
}

/* from pamcmap.h */
union rgb_as_long {
    rgb_pixel rgb;
    unsigned long l;
};

struct hist_item {
	f_pixel acolor;
    double adjusted_weight,   // perceptual weight changed to tweak how mediancut selects colors
          perceptual_weight; // number of pixels weighted by importance of different areas of the picture

    double color_weight; unsigned long sort_value; // these two change every time histogram subset is sorted
};

struct histogram {
    hist_item* achv;
    double total_perceptual_weight;
    uint size;
};

struct colormap_item {
	f_pixel acolor;
	double popularity;
};

struct acolorhist_list_item {
	f_pixel acolor;
	acolorhist_list_item* next;
	double perceptual_weight;
};

int best_color_index(
	const std::vector<colormap_item>& map,
	f_pixel px,
	double* dist_out
	);

struct colormap {
    colormap_item* palette;
    colormap* subset_palette;
    uint colors;
};

struct acolorhist_arr_item {
    rgb_as_long color;
    double perceptual_weight;
};

struct acolorhist_arr_head {
    uint used, capacity;
    acolorhist_arr_item* other_items;
    rgb_as_long color1, color2;
    double perceptual_weight1, perceptual_weight2;
};

struct acolorhash_table {
    mempool* mempool;
    acolorhist_arr_head* buckets;
};

histogram* pam_computeacolorhist(
	const rgb_pixel*const apixels[],
	uint cols, uint rows,
	double gamma,
	uint maxacolors,
	uint ignorebits,
	const double* imp
);
void pam_freeacolorhist(histogram* h);

colormap* pam_colormap(uint colors);
void pam_freecolormap(colormap *c);

