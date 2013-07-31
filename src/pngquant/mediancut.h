#pragma once

colormap*
mediancut(
	histogram* hist,
	const double min_opaque_val,
	uint newcolors,
	const double target_mse
);

