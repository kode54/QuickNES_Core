/* SSE2 version of blitter, outputs 32-bit RGB */

#include "nes_ntsc.h"

#include <stdio.h>
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

/* The AltiVec version also uses this source by overriding the things below */
#ifndef PADDB

#include "emmintrin.h"

enum { bytes_per_pixel = 4 };

typedef __m128i vInt8;

#define LOAD( out, in ) out = _mm_load_si128( &(in) )
#define STORE( out, n ) _mm_stream_si128( out, n )

#define PADDB(   io, in ) io = _mm_add_epi8(  io, in )
#define PADDSB(  io, in ) io = _mm_adds_epi8( io, in )
#define PADDUSB( io, in ) io = _mm_adds_epu8( io, in )

#endif

#include "nes_ntsc_impl3.h"

void nes_ntsc_blit( nes_ntsc_t const* ntsc,
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
		vInt8 v0, v1, v2, v3, v4, v5;
		void* row_end = (char*) out + row_end_off;
		vInt8 const* __restrict kernel;
		
		/* First pixel */
		PIXEL_IN( 0 );
			out = (vInt8*) ((char*) out + out_row_off);
		LOAD( v0, kernel [8+1] );
		LOAD( v1, kernel [8+2] );
		LOAD( v2, kernel [8+3] );
		
		/* Main chunks */
		do
		{
			PIXEL_IN( 1 );
				out += vecs_per_chunk;
			PADDSB( v0, kernel [0+0] );
			PADDB(  v1, kernel [0+1] );
			PADDB(  v2, kernel [0+2] );
			LOAD(   v3, kernel [0+3] );

			PIXEL_IN( 2 );
			PADDSB( v0, kernel [4+0] );
			PADDSB( v1, kernel [4+1] );
			PADDB(  v2, kernel [4+2] );
			PADDB(  v3, kernel [4+3] );

			PIXEL_IN( 3 );
			PADDSB( v1, kernel [8+0] );
			PADDB(  v2, kernel [8+1] );
			PADDB(  v3, kernel [8+2] );
			v4 = v0;
			v5 = v1;
			LOAD(   v0, kernel [8+3] );

			PIXEL_IN( 4 );
			PADDSB( v2, kernel [0+0] );
			PADDB(  v3, kernel [0+1] );
			PADDB(  v0, kernel [0+2] );
			LOAD(   v1, kernel [0+3] );

			PIXEL_IN( 5 );
				in += 6;
			PADDSB( v2, kernel [4+0] );
			PADDSB( v3, kernel [4+1] );
			PADDB(  v0, kernel [4+2] );
			PADDB(  v1, kernel [4+3] );

			PIXEL_IN( 0 );
			PADDSB( v3, kernel [8+0] );
			PADDB(  v0, kernel [8+1] );
			PADDB(  v1, kernel [8+2] );
				PIXELS_OUT( out-(vecs_per_chunk-1), v4, v5, v2, v3 );
			LOAD(   v2, kernel [8+3] );
		}
		while ( (void*) out != row_end );
		
		/* Last pixels */
		PIXEL_IN( 1 );
		PADDSB( v0, kernel [0+0] );
		PADDB(  v1, kernel [0+1] );
		PADDB(  v2, kernel [0+2] );
		LOAD(   v3, kernel [0+3] );

		PIXEL_IN( 2 );
		PADDSB( v0, kernel [4+0] );
		PADDSB( v1, kernel [4+1] );
		PADDB(  v2, kernel [4+2] );
		PADDB(  v3, kernel [4+3] );

		PIXEL_IN( 3 );
			in = (NES_NTSC_IN_T const*) ((char const*) in + in_row_off );
		PADDSB( v1, kernel [8+0] );
		PADDB(  v2, kernel [8+1] );
		PADDB(  v3, kernel [8+2] );
		
		BLACK_IN();
			ktable += burst_offset;
			if ( ktable >= ktable_end )
				ktable -= 3 * burst_offset;
		PADDSB( v2, kernel [0+0] );
		PADDB(  v3, kernel [0+1] );

		PADDSB( v2, kernel [4+0] );
		PADDSB( v3, kernel [4+1] );

		PADDSB( v3, kernel [8+0] );
			PIXELS_OUT( out+1, v0, v1, v2, v3 );
	}
	while ( (void*) out != out_end );
}
