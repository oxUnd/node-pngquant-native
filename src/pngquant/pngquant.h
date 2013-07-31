#ifndef _H_PNGQUANT_H_
#define _H_PNGQUANT_H_

#include "rwpng.h"

int pngquant_exec(struct rwpng_data *in_buffer, struct rwpng_data *out_buffer,int argc, char *argv[]);

#endif