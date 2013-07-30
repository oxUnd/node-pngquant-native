#ifndef _H_PNGQUANT_H_
#define _H_PNGQUANT_H_

#include "rwpng.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    int pngquant(struct rwpng_data *in_buffer, struct rwpng_data *out_buffer,int argc, char *argv[]);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif