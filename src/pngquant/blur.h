#pragma once

//
//  blur.h
//  pngquant

void blur(
	const double* src,
	double* tmp,
	double* dst,
	uint width,
	uint height,
	uint size
);

void max3(
	const double* src,
	double* dst,
	uint width,
	uint height
);

void min3(
	const double* src,
	double* dst,
	uint width,
	uint height
);

