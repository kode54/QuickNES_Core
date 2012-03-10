/* nes_ntsc 0.2.3. http://www.slack.net/~ant/ */

#include "nes_ntsc.h"

/* Copyright (C) 2006-2008 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#define rescale_in      1
#define rescale_out     1
#define rgb_bits        6

#define PACK_RGB( r, g, b ) ((r) << 16 | (g) << 8 | (b))

#define RGB_PALETTE_OUT( rgb, out_ )\
{\
	unsigned char* out = (out_);\
	nes_ntsc_rgb_t temp = rgb ^ rgb_bias;\
	int i;\
	for ( i = 3; --i >= 0; )\
	{\
		int n = (signed char) (temp & 0xFF) << 2;\
		temp >>= 8;\
		if ( n <    0 ) n =    0;\
		if ( n > 0xFF ) n = 0xFF;\
		out [i] = (unsigned char) n;\
	}\
}

#include "nes_ntsc_impl2.h"

/* 3 input pixels -> 8 composite samples */
pixel_info_t const nes_ntsc_pixels [alignment_count] = {
	{ PIXEL_OFFSET( -4, -9 ), { 1, 1, .6667f, 0 } },
	{ PIXEL_OFFSET( -2, -9 ), {       .3333f, 1, 1, .3333f } },
	{ PIXEL_OFFSET(  0, -5 ), {                  0, .6667f, 1, 1 } },
};

#define CORRECT_ERROR2( a ) { out [a] += error - rgb_bias; }
#define DISTRIBUTE_ERROR2( a, b, c ) {\
	nes_ntsc_rgb_t fourth = (error + 2 * nes_ntsc_rgb_builder) >> 2;\
	fourth &= (0xFF >> 2) * nes_ntsc_rgb_builder;\
	fourth -= rgb_bias >> 2;\
	out [a] += fourth;\
	out [b] += fourth;\
	out [c] += fourth;\
	out [i] += error - rgb_bias - (fourth * 3);\
}

static void correct_errors( nes_ntsc_rgb_t color, nes_ntsc_rgb_t* out )
{
	int n;
	for ( n = burst_count; n; --n )
	{
		unsigned i;
		for ( i = 0; i < rgb_kernel_size / 2; i++ )
		{
			nes_ntsc_rgb_t error = color -
					out [ i          ] - 
					out [ i       +16] -
					out [(i+12)%16+32] -
					out [ i+ 8       ] -
					out [ i+ 8    +16] -
					out [ i+ 4    +32];
			
			DISTRIBUTE_ERROR2( i+8, i+8+16, i+4+32 );
			/*CORRECT_ERROR2( i+8 );*/
		}
		
		/* Subtract from last two entries that are added before pixel is output */
		for ( i = 0; i < 4; i++ )
		{
			nes_ntsc_rgb_t const offset = 0x404040;
			out [i     ] -= offset;
			out [i + 16] -= offset;
			out [i+4+16] -= offset;
			out [i + 32] -= offset;
		}
		
		/* Convert to individual signed 8-bit components */
		for ( i = 0; i < alignment_count * rgb_kernel_size; i++ )
			out [i] = (out [i] + rgb_bias) ^ rgb_bias;
		
		out += alignment_count * rgb_kernel_size;
	}
}
