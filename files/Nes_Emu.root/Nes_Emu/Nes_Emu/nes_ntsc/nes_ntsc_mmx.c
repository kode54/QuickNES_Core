/* MMX version of blitter, outputs 32-bit RGB */

#include "nes_ntsc.h"

#include "mmintrin.h"
#include <stddef.h>

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

enum { bytes_per_pixel = 4 };

typedef __m64 vInt8;

//#include "xmmintrin.h"
//#define STORE( out, n ) _mm_stream_pi( out, n )

#define PADDB(   io, in ) io = _m_paddb(   io, in )
#define PADDSB(  io, in ) io = _m_paddsb(  io, in )
#define PADDUSB( io, in ) io = _m_paddusb( io, in )

#include "nes_ntsc_impl3.h"

void nes_ntsc_blit_mmx( nes_ntsc_t const* ntsc,
		NES_NTSC_IN_T const* __restrict in, long in_row_width,
		int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, NES_NTSC_USER_DATA_T user_data )
{
	int const chunk_count = (unsigned) (in_width - nes_ntsc_in_extra) / nes_ntsc_in_chunk;
	ptrdiff_t const out_row_off = out_pitch - nes_ntsc_out_chunk * bytes_per_pixel * chunk_count;
	ptrdiff_t const in_row_off  = (in_row_width - nes_ntsc_in_chunk * chunk_count) * sizeof *in;
	ptrdiff_t const row_end_off = chunk_count * (nes_ntsc_out_chunk * bytes_per_pixel) + out_row_off;
	
	vInt8* __restrict out = (vInt8*) ((char*) rgb_out - out_row_off) - 1;
	void* const out_end = (char*) out + in_height * out_pitch;
	
	char const* const ktable_end = (const char*) ntsc + 3 * burst_offset;
	char const* ktable = (const char*) ntsc + burst_phase * burst_offset;
	
	do
	{
		vInt8 v0, v1, v2, v3, v4, v5, v6, v7;
		void* row_end = (char*) out + row_end_off;
		vInt8 const* __restrict kernel;
		
		/* First pixel */
		PIXEL_IN( 0 );
			out = (vInt8*) ((char*) out + out_row_off);
		LOAD( v0, kernel [16+2] );
		LOAD( v1, kernel [16+3] );
		LOAD( v2, kernel [16+4] );
		LOAD( v3, kernel [16+5] );
		LOAD( v4, kernel [16+6] );
		LOAD( v5, kernel [16+7] );
		
		/* Main chunks */
		do
		{
			PIXEL_IN( 1 );
				out += 8;
			PADDSB( v0, kernel [ 0+0] );
			PADDSB( v1, kernel [ 0+1] );
			PADDB(  v2, kernel [ 0+2] );
			PADDB(  v3, kernel [ 0+3] );
			PADDB(  v4, kernel [ 0+4] );
			PADDB(  v5, kernel [ 0+5] );
			LOAD(   v6, kernel [ 0+6] );
			LOAD(   v7, kernel [ 0+7] );

			PIXEL_IN( 2 );
			PADDSB( v0, kernel [ 8+0] );
			PADDSB( v1, kernel [ 8+1] );
			PADDSB( v2, kernel [ 8+2] );
			PADDSB( v3, kernel [ 8+3] );
			PADDB(  v4, kernel [ 8+4] );
			PADDB(  v5, kernel [ 8+5] );
			PADDB(  v6, kernel [ 8+6] );
			PADDB(  v7, kernel [ 8+7] );

			PIXEL_IN( 3 );
			PADDSB( v2, kernel [16+0] );
			PADDSB( v3, kernel [16+1] );
			PADDB(  v4, kernel [16+2] );
			PADDB(  v5, kernel [16+3] );
			PADDB(  v6, kernel [16+4] );
			PADDB(  v7, kernel [16+5] );
				PIXELS_OUT( out-7, v0, v1, v2, v3 );
			LOAD(   v0, kernel [16+6] );
			LOAD(   v1, kernel [16+7] );

			PIXEL_IN( 4 );
			PADDSB( v4, kernel [ 0+0] );
			PADDSB( v5, kernel [ 0+1] );
			PADDB(  v6, kernel [ 0+2] );
			PADDB(  v7, kernel [ 0+3] );
			PADDB(  v0, kernel [ 0+4] );
			PADDB(  v1, kernel [ 0+5] );
			LOAD(   v2, kernel [ 0+6] );
			LOAD(   v3, kernel [ 0+7] );

			PIXEL_IN( 5 );
				in += 6;
			PADDSB( v4, kernel [ 8+0] );
			PADDSB( v5, kernel [ 8+1] );
			PADDSB( v6, kernel [ 8+2] );
			PADDSB( v7, kernel [ 8+3] );
			PADDB(  v0, kernel [ 8+4] );
			PADDB(  v1, kernel [ 8+5] );
			PADDB(  v2, kernel [ 8+6] );
			PADDB(  v3, kernel [ 8+7] );

			PIXEL_IN( 0 );
			PADDSB( v6, kernel [16+0] );
			PADDSB( v7, kernel [16+1] );
			PADDB(  v0, kernel [16+2] );
			PADDB(  v1, kernel [16+3] );
			PADDB(  v2, kernel [16+4] );
			PADDB(  v3, kernel [16+5] );
				PIXELS_OUT( out-3, v4, v5, v6, v7 );
			LOAD(   v4, kernel [16+6] );
			LOAD(   v5, kernel [16+7] );
		}
		while ( (void*) out != row_end );
		
		/* Last pixels */
		PIXEL_IN( 1 );
		PADDSB( v0, kernel [ 0+0] );
		PADDSB( v1, kernel [ 0+1] );
		PADDB(  v2, kernel [ 0+2] );
		PADDB(  v3, kernel [ 0+3] );
		PADDB(  v4, kernel [ 0+4] );
		PADDB(  v5, kernel [ 0+5] );
		LOAD(   v6, kernel [ 0+6] );
		LOAD(   v7, kernel [ 0+7] );

		PIXEL_IN( 2 );
		PADDSB( v0, kernel [ 8+0] );
		PADDSB( v1, kernel [ 8+1] );
		PADDSB( v2, kernel [ 8+2] );
		PADDSB( v3, kernel [ 8+3] );
		PADDB(  v4, kernel [ 8+4] );
		PADDB(  v5, kernel [ 8+5] );
		PADDB(  v6, kernel [ 8+6] );
		PADDB(  v7, kernel [ 8+7] );

		PIXEL_IN( 3 );
			in = (NES_NTSC_IN_T const*) ((char const*) in + in_row_off );
		PADDSB( v2, kernel [16+0] );
		PADDSB( v3, kernel [16+1] );
		PADDB(  v4, kernel [16+2] );
		PADDB(  v5, kernel [16+3] );
		PADDB(  v6, kernel [16+4] );
		PADDB(  v7, kernel [16+5] );
			PIXELS_OUT( out+1, v0, v1, v2, v3 );

		BLACK_IN();
			ktable += burst_offset;
			if ( ktable >= ktable_end )
				ktable -= 3 * burst_offset;
		PADDSB( v4, kernel [ 0+0] );
		PADDSB( v5, kernel [ 0+1] );
		PADDB(  v6, kernel [ 0+2] );
		PADDB(  v7, kernel [ 0+3] );

		PADDSB( v4, kernel [ 8+0] );
		PADDSB( v5, kernel [ 8+1] );
		PADDSB( v6, kernel [ 8+2] );
		PADDSB( v7, kernel [ 8+3] );

		PADDSB( v6, kernel [16+0] );
		PADDSB( v7, kernel [16+1] );
			PIXELS_OUT( out+5, v4, v5, v6, v7 );
	}
	while ( (void*) out != out_end );
	
	_m_empty();
}
