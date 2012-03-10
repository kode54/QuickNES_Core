
// NES PPU graphics rendering

// Nes_Emu 0.5.0. http://www.slack.net/~ant/

#include "Nes_Ppu.h"

#include <stddef.h>
#include "blargg_endian.h"

/* Copyright (C) 2004-2005 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details. You should have received a copy of the GNU Lesser General
Public License along with this module; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include BLARGG_SOURCE_BEGIN

#ifdef __MWERKS__
	static unsigned zero = 0; // helps CodeWarrior optimizer when added to constants
#else
	unsigned const zero = 0; // compile-time constant on other compilers
#endif

// everything here is speed-critical
#include BLARGG_ENABLE_OPTIMIZER

typedef BOOST::uint32_t uint32_t;
typedef BOOST::uint8_t byte;

template<class T>
inline const T& min( const T& x, const T& y )
{
	if ( x < y )
		return x;
	return y;
}

inline Nes_Ppu_::cached_tile_t const& Nes_Ppu_::get_tile( int i, bool h_flip ) const
{
	return (h_flip ? flipped_tiles : tile_cache)
			[(unsigned) map_chr_addr( i * bytes_per_tile ) / bytes_per_tile];
}

inline uint32_t reorder( uint32_t n )
{
	n |= n << 7;
	return ((n << 14) | n);
}

void Nes_Ppu::update_tiles( int first_tile )
{
	for ( int i = 0; i < 512; i++ )
	{
		if ( !tiles_modified [i] )
			continue;
		
		tiles_modified [i] = false;
		
		const byte* in = chr_rom + (i + first_tile) * bytes_per_tile;
		byte* out = (byte*) tile_cache [i + first_tile];
		byte* flipped_out = (byte*) flipped_tiles [i + first_tile];
		
		static int zero = 0; // helps MWCW optimizer
		const uint32_t bit_mask = 0x11111111 + zero;
		
		for ( int n = 4; n--; )
		{
			uint32_t c =
					((reorder( in [0] ) & bit_mask) << 0) |
					((reorder( in [8] ) & bit_mask) << 1) |
					((reorder( in [1] ) & bit_mask) << 2) |
					((reorder( in [9] ) & bit_mask) << 3);
			in += 2;
			
			SET_BE32( out, c );
			out += 4;
			
			// make horizontally-flipped version
			c =     ((c >> 28) & 0x0000000f) |
					((c >> 20) & 0x000000f0) |
					((c >> 12) & 0x00000f00) |
					((c >>  4) & 0x0000f000) |
					((c <<  4) & 0x000f0000) |
					((c << 12) & 0x00f00000) |
					((c << 20) & 0x0f000000) |
					((c << 28) & 0xf0000000);
			SET_BE32( flipped_out, c );
			flipped_out += 4;
		}
	}
}

// Fill

void Nes_Ppu::black_background( int begin, int end )
{
	ptrdiff_t const next_line = row_bytes - image_width;
	uint32_t* pixels = (uint32_t*) (base_pixels + begin * row_bytes);
	
	uint32_t fill = palette_offset;
	if ( (vram_addr & 0x3f00) == 0x3f00 )
	{
		// PPU uses current palette entry if addr is within palette ram
		int color = vram_addr & 0x1f;
		if ( !(color & 3) )
			color &= ~0x10;
		fill += color * 0x01010101;
	}
	
	for ( int n = end - begin; n--; )
	{
		for ( int n = image_width / 16; n--; )
		{
			pixels [0] = fill;
			pixels [1] = fill;
			pixels [2] = fill;
			pixels [3] = fill;
			pixels += 4;
		}
		pixels = (uint32_t*) ((byte*) pixels + next_line);
	}
}

void Nes_Ppu::clip_left( int begin, int end )
{
	ptrdiff_t next_line = row_bytes;
	byte* p = base_pixels + row_bytes * begin;
	uint32_t fill = palette_offset;
	
	for ( int n = end - begin; n--; )
	{
		((uint32_t*) p) [0] = fill;
		((uint32_t*) p) [1] = fill;
		p += next_line;
	}
}

void Nes_Ppu::save_left( int begin, int end )
{
	ptrdiff_t next_line = row_bytes;
	byte* in = base_pixels + row_bytes * begin;
	uint32_t* out = impl->clip_buf;
	
	for ( int n = end - begin; n--; )
	{
		uint32_t in0 = ((uint32_t*) in) [0];
		uint32_t in1 = ((uint32_t*) in) [1];
		in += next_line;
		out [0] = in0;
		out [1] = in1;
		out += 2;
	}
}

void Nes_Ppu::restore_left( int begin, int end )
{
	ptrdiff_t next_line = row_bytes;
	byte* out = base_pixels + row_bytes * begin;
	uint32_t* in = impl->clip_buf;
	
	for ( int n = end - begin; n--; )
	{
		uint32_t in0 = in [0];
		uint32_t in1 = in [1];
		in += 2;
		((uint32_t*) out) [0] = in0;
		((uint32_t*) out) [1] = in1;
		out += next_line;
	}
}

// Background

template<int clipped>
void draw_bg_row( Nes_Ppu_& ppu, int scanline, int skip, int height )
{
	ptrdiff_t const row_bytes = ppu.row_bytes;
	byte* pixels = ppu.base_pixels + scanline * row_bytes - ppu.pixel_x;
	
	int vaddr = ppu.vram_addr;
	ppu.vram_addr = vaddr;
	int y = (vaddr >> 5) & 31;
	int attr_addr = 0x3c0 + (y >> 2) * 8;
	int attr_shift = ((y << 1) & 4);
	int addr = vaddr & 0x3ff;
	
	int odd_start = skip & 1;
	height -= odd_start;
	int pair_count = height >> 1;
	
	// split row into two groups if it crosses nametable
	int count = 32 - (addr & 31);
	int remain = (ppu.pixel_x ? 33 : 32) - count;
	for ( int n = 2; n--; )
	{
		byte const* nametable = ppu.get_nametable( vaddr );
		int bg_bank = (ppu.w2000 << 4) & 0x100;
		uint32_t const mask = 0x03030303 + zero;
		uint32_t const attrib_factor = 0x04040404 + zero;
		while ( count-- )
		{
			int attrib = nametable [attr_addr + ((addr >> 2) & 7)];
			attrib >>= attr_shift + (addr & 2);
			uint32_t offset = (attrib & 3) * attrib_factor + ppu.palette_offset;
			
			// draw one tile
			Nes_Ppu_::cache_t const* lines =
					ppu.get_tile( nametable [addr] + bg_bank );
			byte* p = pixels;
			if ( clipped )
			{
				lines += skip >> 1;
				
				if ( skip & 1 )
				{
					uint32_t line = *lines++;
					(uint32_t&) p [0] = ((line >> 6) & mask) + offset;
					(uint32_t&) p [4] = ((line >> 2) & mask) + offset;
					p += row_bytes;
				}
				
				for ( int n = pair_count; n--; )
				{
					uint32_t line = *lines++;
					(uint32_t&) p [0] = ((line >> 4) & mask) + offset;
					(uint32_t&) p [4] = ((line     ) & mask) + offset;
					p += row_bytes;
					(uint32_t&) p [0] = ((line >> 6) & mask) + offset;
					(uint32_t&) p [4] = ((line >> 2) & mask) + offset;
					p += row_bytes;
				}
				
				if ( height & 1 )
				{
					uint32_t line = *lines++;
					(uint32_t&) p [0] = ((line >> 4) & mask) + offset;
					(uint32_t&) p [4] = ((line     ) & mask) + offset;
				}
			}
			else
			{
				// optimal case: no clipping
				for ( int n = 4; n--; )
				{
					uint32_t line = *lines++;
					(uint32_t&) p [0] = ((line >> 4) & mask) + offset;
					(uint32_t&) p [4] = ((line     ) & mask) + offset;
					p += row_bytes;
					(uint32_t&) p [0] = ((line >> 6) & mask) + offset;
					(uint32_t&) p [4] = ((line >> 2) & mask) + offset;
					p += row_bytes;
				}
			} 
			
			pixels += 8; // next tile
			addr++;
		}
		
		count = remain;
		addr -= 32;
		vaddr ^= 0x400;
	}
}

void Nes_Ppu::draw_background( int first, int last )
{
	vram_addr = (vram_addr & ~0x41f) | (vram_temp & 0x41f);
	while ( first < last )
	{
		int y = vram_addr >> 12;
		int height = min( last - first, 8 - y );
		if ( y == 0 && height == 8 )
			draw_bg_row<0>( *this, first, y, height );
		else
			draw_bg_row<1>( *this, first, y, height );
		
		run_hblank( min( last - first - 1, height ) );
		first += height;
	}
}

// Sprites

template<int clipped>
void ppu_draw_sprite( Nes_Ppu_& ppu, byte const* sprite, int begin, int end )
{
	int top = sprite [0] + 1;
	int tall = (ppu.w2000 >> 5) & 1;
	int height = 1 << (tall + 3);
	
	// should be visible
	assert( (top + height) > begin && top < end );
	
	int skip;
	int visible;
	if ( !clipped )
	{
		// shouldn't require any clipping
		assert( begin <= top && top + height <= end );
	}
	else
	{
		// should be somewhat clipped
		assert( top < begin || top + height > end );
		skip = begin - top;
		if ( skip < 0 )
			skip = 0;
		
		int skip_bottom = top + height - end;
		if ( skip_bottom < 0 )
			skip_bottom = 0;
		
		visible = height - skip - skip_bottom;
		assert( visible > 0 );
	}
	
	int dir = 1;
	byte* scanlines = ppu.sprite_scanlines + (clipped ? top + skip : top);
	
	// dest
	ptrdiff_t next_row = ppu.row_bytes;
	byte* out = ppu.base_pixels + (clipped ? top + skip : top) * next_row + sprite [3];
	if ( sprite [2] & 0x80 )
	{
		// vertical flip
		out += (clipped ? next_row * visible : next_row << (tall + 3)) - next_row;
		next_row = -next_row;
		dir = -1;
		scanlines += (clipped ? visible : 8 << tall) - 1;
		if ( clipped )
		{
			skip = height - skip - visible;
			assert( skip + visible <= height );
		}
	}
	
	// source
	int tile = sprite [1] + ((ppu.w2000 << 5) & 0x100);
	if ( tall )
		tile = (tile & 1) * 0x100 + (tile & 0xfe);
	Nes_Ppu_::cache_t const* lines = ppu.get_tile( tile, sprite [2] & 0x40 );
	if ( clipped )
		lines += skip >> 1;
	
	// attributes
	uint32_t offset =  (sprite [2] & 3) * 0x04040404 + (ppu.palette_offset + 0x10101010);
	
	uint32_t const mask    = 0x03030303 + zero;
	uint32_t const maskgen = 0x80808080 + zero;
	
	#define DRAW_LINE { \
		int sprite_count = *scanlines;  \
		CALC_FOUR( (uint32_t&) out [0], (line >> 4), out0 ) \
		CALC_FOUR( (uint32_t&) out [4], line, out1 )    \
		*scanlines = sprite_count + 1;  \
		scanlines += dir;               \
		if ( sprite_count < 64 ) {      \
			(uint32_t&) out [0] = out0; \
			(uint32_t&) out [4] = out1; \
		}                               \
		out += next_row;                \
	}
		
	if ( sprite [2] & 0x20 )
	{
		// behind
		uint32_t const omask = 0x20202020 + zero;
		uint32_t const bg_or = 0xc3c3c3c3 + zero;
	
		#define CALC_FOUR( in, line, out )                      \
			uint32_t out;                                       \
			{                                                   \
				uint32_t bg = (uint32_t&) in;                   \
				uint32_t sp = line & mask;                      \
				uint32_t bgm = maskgen - (bg & mask);           \
				uint32_t spm = maskgen - sp;                    \
				out = (bg & (bgm | bg_or)) | (spm & omask) |    \
						(((offset & spm) + sp) & ~(bgm >> 2));  \
			}

		if ( clipped && skip & 1 )
		{
			visible--;
			uint32_t line = *lines++ >> 2;
			DRAW_LINE
		}
		
		for ( int n = clipped ? (visible >> 1) : (4 << tall); n--; )
		{
			uint32_t line = *lines++;
			DRAW_LINE
			line >>= 2;
			DRAW_LINE
		}
		
		if ( clipped && visible & 1 )
		{
			uint32_t line = *lines;
			DRAW_LINE
		}
		
		#undef CALC_FOUR
	}
	else
	{
		uint32_t const maskgen2 = 0x7f7f7f7f + zero;

		#define CALC_FOUR( in, line, out )                      \
			uint32_t out;                                       \
			{                                                   \
				uint32_t bg = in;                               \
				uint32_t sp = line & mask;                      \
				uint32_t bgm = maskgen2 + ((bg >> 4) & mask);   \
				uint32_t spm = (maskgen - sp) & maskgen2;       \
				uint32_t m = (bgm & spm) >> 2;                  \
				out = (bg & ~m) | ((sp + offset) & m);          \
			}
		
		if ( clipped && skip & 1 )
		{
			visible--;
			uint32_t line = *lines++ >> 2;
			DRAW_LINE
		}
		
		for ( int n = clipped ? (visible >> 1) : (4 << tall); n--; )
		{
			uint32_t line = *lines++;
			DRAW_LINE
			line >>= 2;
			DRAW_LINE
		}
		
		if ( clipped && visible & 1 )
		{
			uint32_t line = *lines;
			DRAW_LINE
		}
	}
}

void Nes_Ppu::draw_sprites( int begin, int end )
{
	int height_plus_one = ((w2000 >> 2) & 8) + (8 + 1);
	for ( byte const* sprite = spr_ram; sprite != spr_ram + 0x100; sprite += 4 )
	{
		int top = sprite [0] + 1;
		int bottom = sprite [0] + height_plus_one;
		
		if ( ((begin - bottom) & (top - end)) < 0 ) // optimize both comparisons to one
		{
			// sprite is visible
			if ( ((top - begin) | (end - bottom)) >= 0 )
				ppu_draw_sprite<0>( *this, sprite, begin, end ); // needs no clipping
			else
				ppu_draw_sprite<1>( *this, sprite, begin, end ); // clipped
		}
	}
}

