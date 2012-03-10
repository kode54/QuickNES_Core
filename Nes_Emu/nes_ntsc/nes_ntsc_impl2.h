/* Common to nes_ntsc and nes_ntsc (vector-optimized version) */

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

nes_ntsc_setup_t const nes_ntsc_monochrome = { 0,-1, 0, 0,.2,  0,.2,-.2,-.2,-1, 1, 0, 0, 0, 0 };
nes_ntsc_setup_t const nes_ntsc_composite  = { 0, 0, 0, 0, 0,  0, 0,  0,  0, 0, 1, 0, 0, 0, 0 };
nes_ntsc_setup_t const nes_ntsc_svideo     = { 0, 0, 0, 0,.2,  0,.2, -1, -1, 0, 1, 0, 0, 0, 0 };
nes_ntsc_setup_t const nes_ntsc_rgb        = { 0, 0, 0, 0,.2,  0,.7, -1, -1,-1, 1, 0, 0, 0, 0 };

#define alignment_count 3
#define burst_count     3

#define artifacts_mid   1.0f
#define fringing_mid    1.0f
#define std_decoder_hue -15

#define STD_HUE_CONDITION( setup ) !(setup->base_palette || setup->palette)

#include "nes_ntsc_impl.h"

static void correct_errors( nes_ntsc_rgb_t color, nes_ntsc_rgb_t* out );

static void merge_kernel_fields( nes_ntsc_rgb_t* io )
{
	int n;
	for ( n = burst_size; n; --n )
	{
		nes_ntsc_rgb_t p0 = io [burst_size * 0] + rgb_bias;
		nes_ntsc_rgb_t p1 = io [burst_size * 1] + rgb_bias;
		nes_ntsc_rgb_t p2 = io [burst_size * 2] + rgb_bias;
		/* merge colors without losing precision */
		io [burst_size * 0] =
				((p0 + p1 - ((p0 ^ p1) & nes_ntsc_rgb_builder)) >> 1) - rgb_bias;
		io [burst_size * 1] =
				((p1 + p2 - ((p1 ^ p2) & nes_ntsc_rgb_builder)) >> 1) - rgb_bias;
		io [burst_size * 2] =
				((p2 + p0 - ((p2 ^ p0) & nes_ntsc_rgb_builder)) >> 1) - rgb_bias;
		++io;
	}
}

void nes_ntsc_init( nes_ntsc_t* ntsc, nes_ntsc_setup_t const* setup )
{
	int merge_fields;
	int entry;
	init_t impl;
	float gamma_factor;
	
	if ( !setup )
		setup = &nes_ntsc_composite;
	init( &impl, setup );
	
	/* setup fast gamma */
	{
		float gamma = (float) setup->gamma * -0.5f;
		if ( STD_HUE_CONDITION( setup ) )
			gamma += 0.1333f;
		
		gamma_factor = (float) pow( (float) fabs( gamma ), 0.73f );
		if ( gamma < 0 )
			gamma_factor = -gamma_factor;
	}
	
	merge_fields = setup->merge_fields;
	if ( setup->artifacts <= -1 && setup->fringing <= -1 )
		merge_fields = 1;
	
	for ( entry = 0; entry < nes_ntsc_palette_size; entry++ )
	{
		/* Base 64-color generation */
		static float const lo_levels [4] = { -0.12f, 0.00f, 0.31f, 0.72f };
		static float const hi_levels [4] = {  0.40f, 0.68f, 1.00f, 1.00f };
		int level = entry >> 4 & 0x03;
		float lo = lo_levels [level];
		float hi = hi_levels [level];
		
		int color = entry & 0x0F;
		if ( color == 0 )
			lo = hi;
		if ( color == 0x0D )
			hi = lo;
		if ( color > 0x0D )
			hi = lo = 0.0f;
		
		{
			/* phases [i] = cos( i * PI / 6 ) */
			static float const phases [0x10 + 3] = {
				-1.0f, -0.866025f, -0.5f, 0.0f,  0.5f,  0.866025f,
				 1.0f,  0.866025f,  0.5f, 0.0f, -0.5f, -0.866025f,
				-1.0f, -0.866025f, -0.5f, 0.0f,  0.5f,  0.866025f,
				 1.0f
			};
			#define TO_ANGLE_SIN( color )   phases [color]
			#define TO_ANGLE_COS( color )   phases [(color) + 3]
			
			/* Convert raw waveform to YIQ */
			float sat = (hi - lo) * 0.5f;
			float i = TO_ANGLE_SIN( color ) * sat;
			float q = TO_ANGLE_COS( color ) * sat;
			float y = (hi + lo) * 0.5f;
			
			/* Optionally use base palette instead */
			if ( setup->base_palette )
			{
				unsigned char const* in = &setup->base_palette [(entry & 0x3F) * 3];
				static float const to_float = 1.0f / 0xFF;
				float r = to_float * in [0];
				float g = to_float * in [1];
				float b = to_float * in [2];
				q = RGB_TO_YIQ( r, g, b, y, i );
			}
			
			/* Apply color emphasis */
			#ifdef NES_NTSC_EMPHASIS
			{
				int tint = entry >> 6 & 7;
				if ( tint && color <= 0x0D )
				{
					static float const atten_mul = 0.79399f;
					static float const atten_sub = 0.0782838f;
					
					if ( tint == 7 )
					{
						y = y * (atten_mul * 1.13f) - (atten_sub * 1.13f);
					}
					else
					{
						static unsigned char const tints [8] = { 0, 6, 10, 8, 2, 4, 0, 0 };
						int const tint_color = tints [tint];
						float sat = hi * (0.5f - atten_mul * 0.5f) + atten_sub * 0.5f;
						y -= sat * 0.5f;
						if ( tint >= 3 && tint != 4 )
						{
							/* combined tint bits */
							sat *= 0.6f;
							y -= sat;
						}
						i += TO_ANGLE_SIN( tint_color ) * sat;
						q += TO_ANGLE_COS( tint_color ) * sat;
					}
				}
			}
			#endif
			
			/* Optionally use palette instead */
			if ( setup->palette )
			{
				unsigned char const* in = &setup->palette [entry * 3];
				static float const to_float = 1.0f / 0xFF;
				float r = to_float * in [0];
				float g = to_float * in [1];
				float b = to_float * in [2];
				q = RGB_TO_YIQ( r, g, b, y, i );
			}
			
			/* Apply brightness, contrast, and gamma */
			y *= (float) setup->contrast * 0.5f + 1;
			/* adjustment reduces error when using input palette */
			y += (float) setup->brightness * 0.5f - 0.5f / 256;
			
			{
				float r, g, b = YIQ_TO_RGB( y, i, q, default_decoder, float, r, g );
				
				/* fast approximation of n = pow( n, gamma ) */
				r = (r * gamma_factor - gamma_factor) * r + r;
				g = (g * gamma_factor - gamma_factor) * g + g;
				b = (b * gamma_factor - gamma_factor) * b + b;
				
				q = RGB_TO_YIQ( r, g, b, y, i );
			}
			
			i *= rgb_unit;
			q *= rgb_unit;
			y *= rgb_unit;
			y += rgb_offset;
			
			/* Generate kernel */
			{
				int r, g, b = YIQ_TO_RGB( y, i, q, impl.to_rgb, int, r, g );
				/* blue tends to overflow, so clamp it */
				nes_ntsc_rgb_t rgb = PACK_RGB( r, g, (b < 0x3E0 ? b: 0x3E0) );
				
				if ( setup->palette_out )
					RGB_PALETTE_OUT( rgb, &setup->palette_out [entry * 3] );
				
				if ( ntsc )
				{
					nes_ntsc_rgb_t* kernel = ntsc->table [entry];
					gen_kernel( &impl, y, i, q, kernel );
					if ( merge_fields )
						merge_kernel_fields( kernel );
					correct_errors( rgb, kernel );
				}
			}
		}
	}
}
