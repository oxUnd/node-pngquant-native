#pragma once

// from http://www001.upp.so-net.ne.jp/isaku/dxor.c.html

#include <emmintrin.h>

static union { int i[4]; double d[2]; __m128i m; }
x = {{123456789,(362436069&0xFFFFF)|0x3FF00000,0,0}},
y = {{521288629,( 88675123&0xFFFFF)|0x3FF00000,0,0}},
z = {{  5783321,(  6615241&0xFFFFF)|0x3FF00000,0,0}};

__inline
void sdxor156(int s)
{
	// http://unkar.org/r/tech/1192628099#l76
	x.i[0] = s=1812433253UL*(s^(s>>30)) + 1;
	y.i[0] = s=1812433253UL*(s^(s>>30)) + 1;
	z.i[0] = s=1812433253UL*(s^(s>>30)) + 1;
}

__inline double dxor156(void) {
    const union { int i[4]; __m128i m; } mask={{-1,0xFFFFF,0,0}};
    __m128i t=x.m, s;
    s=_mm_slli_epi64(t,19);     t=_mm_xor_si128(t,s);
    t=_mm_and_si128(t,mask.m);  s=_mm_srli_epi64(t,33);
    s=_mm_xor_si128(s,t); t=y.m; x.m=t; t=z.m; y.m=t;
    t=_mm_xor_si128(t,s); z.m=t; return z.d[0]-1.0;
}
