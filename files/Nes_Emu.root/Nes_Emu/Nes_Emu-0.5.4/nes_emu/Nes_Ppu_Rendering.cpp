
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Ppu_Rendering.h"

#include <stddef.h>

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

#include BLARGG_ENABLE_OPTIMIZER

typedef BOOST::uint32_t uint32_t;
typedef BOOST::uint8_t byte;

#ifdef __MWERKS__
	static unsigned zero = 0; // helps CodeWarrior optimizer when added to constants
#else
	unsigned const zero = 0; // compile-time constant on other compilers
#endif

// Pixel filling

void Nes_Ppu_Rendering::fill_background( int count )
{
	ptrdiff_t const next_line = scanline_row_bytes - image_width;
	uint32_t* pixels = (uint32_t*) scanline_pixels;
	
	uint32_t fill = palette_offset;
	if ( (vram_addr & 0x3f00) == 0x3f00 )
	{
		// PPU uses current palette entry if addr is within palette ram
		int color = vram_addr & 0x1f;
		if ( !(color & 3) )
			color &= ~0x10;
		fill += color * 0x01010101;
	}
	
	for ( int n = count; n--; )
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

void Nes_Ppu_Rendering::clip_left( int count )
{
	ptrdiff_t next_line = scanline_row_bytes;
	byte* p = scanline_pixels;
	uint32_t fill = palette_offset;
	
	for ( int n = count; n--; )
	{
		((uint32_t*) p) [0] = fill;
		((uint32_t*) p) [1] = fill;
		p += next_line;
	}
}

void Nes_Ppu_Rendering::save_left( int count )
{
	ptrdiff_t next_line = scanline_row_bytes;
	byte* in = scanline_pixels;
	uint32_t* out = impl->clip_buf;
	
	for ( int n = count; n--; )
	{
		uint32_t in0 = ((uint32_t*) in) [0];
		uint32_t in1 = ((uint32_t*) in) [1];
		in += next_line;
		out [0] = in0;
		out [1] = in1;
		out += 2;
	}
}

void Nes_Ppu_Rendering::restore_left( int count )
{
	ptrdiff_t next_line = scanline_row_bytes;
	byte* out = scanline_pixels;
	uint32_t* in = impl->clip_buf;
	
	for ( int n = count; n--; )
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
inline void Nes_Ppu_Rendering_<clipped>::draw_bg( Nes_Ppu_Rendering& ppu, int scanline,
		int skip, int height )
{
	ptrdiff_t const row_bytes = ppu.scanline_row_bytes;
	byte* pixels = ppu.scanline_pixels + scanline * row_bytes - ppu.pixel_x;
	
	int vaddr = ppu.vram_addr;
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
			Nes_Ppu_Rendering::cache_t const* lines =
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

void draw_bg_unclipped( Nes_Ppu_Rendering& ppu, int scanline, int skip, int height )
{
	Nes_Ppu_Rendering_<0>::draw_bg( ppu, scanline, skip, height );
}

void draw_bg_clipped( Nes_Ppu_Rendering& ppu, int scanline, int skip, int height )
{
	Nes_Ppu_Rendering_<1>::draw_bg( ppu, scanline, skip, height );
}

// Sprites

template<int clipped>
void Nes_Ppu_Rendering_<clipped>::draw_sprite( Nes_Ppu_Rendering& ppu, byte const* sprite,
		int begin, int end )
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
	ptrdiff_t next_row = ppu.scanline_row_bytes;
	byte* out = ppu.scanline_pixels + sprite [3] +
			((clipped ? top + skip : top) - begin) * next_row;
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
	Nes_Ppu_Rendering::cache_t const* lines = ppu.get_tile( tile, sprite [2] & 0x40 );
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
		if ( sprite_count < ppu.max_sprites ) { \
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

void Nes_Ppu_Rendering::check_sprite_hit( int begin, int end )
{
	int sprite_height = this->sprite_height();
	
	int top = spr_ram [0] + 1;
	int bottom = top + sprite_height;
	
	int off_top = begin - top;
	if ( off_top < 0 )
		off_top = 0;
	top += off_top;
	
	if ( bottom > end )
		bottom = end;
	
	if ( top >= bottom )
		return;
	
	int visible = bottom - top;
	
	// save and compare pixels into buffer
	uint32_t buf [16 * 2];
	int sprite_x = spr_ram [3];
	if ( sprite_x < 8 && (w2001 & 0x01e) != 0x1e )
		sprite_x = 8; // ignore left 8-pixel band if clipping is enabled
	if ( sprite_x > 247 ) // don't check pixel 255 and beyond
		sprite_x = 247;
	byte const* bg = scanline_pixels + (top - begin) * scanline_row_bytes + sprite_x;
	
	// save pixels under sprite and pre-mask left edge if clipping is enabled
	{
		byte const* in = bg;
		uint32_t* out = buf;
		ptrdiff_t const row_bytes = this->scanline_row_bytes;
		unsigned long mask = 0x03030303 + zero;
		unsigned long gen  = 0x80808080 + zero;
		for ( int n = visible; n--; )
		{
			unsigned long in0 = (uint32_t&) in [0];
			unsigned long in1 = (uint32_t&) in [4];
			in += row_bytes;
			
			out [0] = gen - (in0 & mask);
			out [1] = gen - (in1 & mask);
			out += 2;
		}
	}
	
	if ( off_top | (visible - sprite_height) )
		Nes_Ppu_Rendering_<1>::draw_sprite( *this, spr_ram, begin, end );
	else
		Nes_Ppu_Rendering_<0>::draw_sprite( *this, spr_ram, begin, end );
	
	// find any previously non-transparent pixels that now have sprite or front flag set
	byte const* after = bg;
	uint32_t const* before = buf;
	ptrdiff_t const row_bytes = this->scanline_row_bytes;
	unsigned long mask = 0x30303030 + zero;
	for ( int i = 0; i < visible; i++ )
	{
		unsigned long hit0 = before [0] & (uint32_t&) after [0];
		unsigned long hit1 = before [1] & (uint32_t&) after [4];
		after += row_bytes;
		before += 2;
		
		if ( (hit0 | hit1) & mask )
		{
			// hit detected; determine X position
			
			int x = sprite_x;
			hit0 &= mask;
			if ( !hit0 )
			{
				hit0 = hit1 & mask;
				x += 4;
			}
			uint32_t hit = hit0;
			byte* p = (byte*) &hit;
			
			if ( !p [0] )
			{
				if ( p [1] )
					x += 1;
				else if ( p [2] )
					x += 2;
				else
					x += 3;
			}
			
			assert( x < 255 );
			sprite_hit_y = top - off_top + i;
			sprite_hit_x = x;
			break;
		}
	}
}

void Nes_Ppu_Rendering::draw_sprites( int begin, int end )
{
	int starting_sprite = 0;
	if ( !sprite_hit_y )
	{
		check_sprite_hit( begin, end );
		starting_sprite = 4;
	}
	
	int height_plus_one = this->sprite_height() + 1;
	for ( byte const* sprite = spr_ram + starting_sprite; sprite != spr_ram + 0x100; sprite += 4 )
	{
		int top = sprite [0] + 1;
		int bottom = sprite [0] + height_plus_one;
		
		if ( ((begin - bottom) & (top - end)) < 0 ) // optimize both comparisons to one
		{
			// sprite is visible
			if ( ((top - begin) | (end - bottom)) >= 0 )
				Nes_Ppu_Rendering_<0>::draw_sprite( *this, sprite, begin, end ); // needs no clipping
			else
				Nes_Ppu_Rendering_<1>::draw_sprite( *this, sprite, begin, end ); // clipped
		}
	}
}

