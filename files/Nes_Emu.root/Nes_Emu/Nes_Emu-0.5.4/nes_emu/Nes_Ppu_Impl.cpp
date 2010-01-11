
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Ppu_Impl.h"

#include <string.h>
#include "blargg_endian.h"
#include "Nes_Snapshot.h"

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

int const cache_line_size = 128; //  tile cache is kept aligned to this boundary

Nes_Ppu_Impl::Nes_Ppu_Impl()
{
	impl = NULL;
	chr_rom = NULL;
	chr_size = 0;
	tile_cache = NULL;
	mapper = NULL;
	tile_cache_mem = NULL;
	caller_pixels = NULL;
	set_sprite_limit( 8 );
	set_palette_start( palette_size );
	set_pixels( NULL, 0 );
	memset( unused, 0, sizeof unused );
}

Nes_Ppu_Impl::~Nes_Ppu_Impl()
{
	close_chr();
	delete impl;
}

void Nes_Ppu_Impl::all_tiles_modified()
{
	any_tiles_modified = true;
	memset( modified_tiles, ~0, sizeof modified_tiles );
}

blargg_err_t Nes_Ppu_Impl::open_chr( byte const* new_chr, long chr_rom_size )
{
	close_chr();
	
	if ( !impl )
	{
		impl = BLARGG_NEW impl_t;
		BLARGG_CHECK_ALLOC( impl );
	}
	
	chr_rom = new_chr;
	chr_size = chr_rom_size;
	chr_is_writable = false;
	
	if ( chr_rom_size == 0 )
	{
		// CHR RAM
		chr_rom = impl->chr_ram;
		chr_size = sizeof impl->chr_ram;
		chr_is_writable = true;
	}
	
	// allocate aligned memory for cache
	assert( chr_size % chr_addr_size == 0 );
	long tile_count = chr_size / bytes_per_tile;
	tile_cache_mem = BLARGG_NEW byte [tile_count * sizeof (cached_tile_t) * 2 + cache_line_size];
	tile_cache = (cached_tile_t*) (tile_cache_mem + cache_line_size -
			(unsigned long) tile_cache_mem % cache_line_size);
	flipped_tiles = tile_cache + tile_count;
	
	// rebuild cache
	all_tiles_modified();
	if ( !chr_is_writable )
	{
		any_tiles_modified = false;
		for ( int i = chr_size / bytes_per_tile; i--; )
			update_tile( i );
	}
	
	return blargg_success;
}

void Nes_Ppu_Impl::close_chr()
{
	delete [] tile_cache_mem;
	tile_cache_mem = NULL;
}

void Nes_Ppu_Impl::set_pixels( void* p, long rb )
{
	caller_pixels = NULL;
	caller_row_bytes = rb;
	if ( p )
		caller_pixels = (byte*) p + rb * image_top;
}

void Nes_Ppu_Impl::set_chr_bank( int addr, int size, long data )
{
	check( !chr_is_writable || addr == data ); // to do: is CHR RAM ever bank-switched?
	//dprintf( "Tried to set CHR RAM bank at %04X to CHR+%04X\n", addr, data );
	
	if ( data + size > chr_size )
		data %= chr_size;
	
	int count = (unsigned) size / chr_page_size;
	assert( chr_page_size * count == size );
	assert( addr + size <= chr_addr_size );
	
	int page = (unsigned) addr / chr_page_size;
	while ( count-- )
	{
		chr_pages [page++] = data;
		data += chr_page_size;
	}
}

void Nes_Ppu_Impl::save_state( Nes_Snapshot* out ) const
{
	out->ppu = *this;
	out->ppu_valid = true;
	
	memcpy( out->spr_ram, spr_ram, sizeof out->spr_ram );
	out->spr_ram_valid = true;
	
	out->nametable_size = (nt_banks_ [3] == 3 ? 0x1000 : 0x800);
	memcpy( out->nametable, impl->nt_ram, out->nametable_size );
	
	out->chr_size = 0;
	if ( chr_is_writable )
	{
		out->chr_size = chr_size;
		assert( out->chr_size <= sizeof out->chr);
		memcpy( out->chr, impl->chr_ram, out->chr_size );
	}
}

void Nes_Ppu_Impl::load_state( Nes_Snapshot const& in )
{
	if ( in.ppu_valid )
		STATIC_CAST(ppu_state_t&) (*this) = in.ppu;
	
	if ( in.spr_ram_valid )
		memcpy( spr_ram, in.spr_ram, sizeof spr_ram );
	
	assert( in.nametable_size <= sizeof impl->nt_ram );
	memcpy( impl->nt_ram, in.nametable, in.nametable_size );
	
	if ( chr_is_writable && in.chr_size )
	{
		assert( in.chr_size <= sizeof impl->chr_ram );
		memcpy( impl->chr_ram, in.chr, in.chr_size );
		all_tiles_modified();
	}
}

void Nes_Ppu_Impl::dma_sprites( void const* in )
{
	//if ( w2003 != 0 )
	//  dprintf( "Sprite DMA when $2003 = %02X\n", (int) w2003 );
	
	memcpy( spr_ram + w2003, in, 0x100 - w2003 );
	memcpy( spr_ram, (char*) in + 0x100 - w2003, w2003 );
}

static BOOST::uint8_t const initial_palette [0x20] =
{
	0x0f,0x01,0x00,0x01,0x00,0x02,0x02,0x0D,
	0x08,0x10,0x08,0x24,0x00,0x00,0x04,0x2C,
	0x00,0x01,0x34,0x03,0x00,0x04,0x00,0x14,
	0x00,0x3A,0x00,0x02,0x00,0x20,0x2C,0x08
};

void Nes_Ppu_Impl::reset()
{
	set_nt_banks( 0, 0, 0, 0 );
	set_chr_bank( 0, chr_addr_size, 0 );
	memset( impl->nt_ram, 0, sizeof impl->nt_ram );
	memcpy( palette, initial_palette, sizeof palette );
	memset( spr_ram, 0, sizeof spr_ram );
	memset( impl->chr_ram, 0, sizeof impl->chr_ram );
	all_tiles_modified();
}

void Nes_Ppu_Impl::run_hblank( int count )
{
	require( count >= 0 );
	
	if ( w2001 & 0x08 )
	{
		long addr = vram_addr;
		
		while ( count-- > 0 )
		{
			addr += 0x1000;
			if ( addr >= 0x8000 )
			{
				int y = (addr + 0x20) & 0x3e0;
				addr &= 0xfff & ~0x3e0;
				if ( y == 30 * 0x20 )
					y = 0x800;
				addr ^= y;
			}
			
			addr = (addr & ~0x41f) | (vram_temp & 0x41f);
		}
		vram_addr = addr;
	}
}

void Nes_Ppu_Impl::first_scanline()
{
	memcpy( host_palette, palette, 32 );
	int bg = palette [0];
	for ( int i = 0; i < 32; i += 4 )
		host_palette [i] = bg;
	memcpy( host_palette + 32, host_palette, 32 );
	
	memset( sprite_scanlines, starting_sprites_per_scanline_count(),
			sizeof sprite_scanlines );
}

void Nes_Ppu_Impl::render_scanlines( int start, int count, byte* pixels, long pitch )
{
	assert( start + count <= image_height );
	assert( pixels );
	
	scanline_pixels = pixels + image_left;
	scanline_row_bytes = pitch;
	
	int const obj_mask = 2;
	int const bg_mask = 1;
	int draw_mode = (w2001 >> 3) & 3;
	int clip_mode = (~w2001 >> 1) & draw_mode;
	
	if ( !(draw_mode & bg_mask) )
	{
		// no background
		run_hblank( count - 1 );
		fill_background( count );
		clip_mode |= bg_mask; // avoid unnecessary save/restore
	}
	
	if ( draw_mode )
	{
		// sprites and/or background are being rendered
		
		if ( any_tiles_modified )
		{
			any_tiles_modified = false;
			update_tiles( 0 );
		}
	
		if ( draw_mode & bg_mask )
		{
			draw_background( count );
			
			if ( clip_mode == bg_mask )
				clip_left( count );
		}
		
		if ( draw_mode & obj_mask )
		{
			// when clipping just sprites, save left strip then restore after drawing sprites
			if ( clip_mode == obj_mask )
				save_left( count );
				
			draw_sprites( start, start + count );
			
			if ( clip_mode == obj_mask )
				restore_left( count );
		}
		
		if ( clip_mode == (obj_mask | bg_mask) )
			clip_left( count );
	}
	
	scanline_pixels = NULL;
}

void Nes_Ppu_Impl::draw_background( int last )
{
	int first = 0;
	while ( first < last )
	{
		int y = vram_addr >> 12;
		int height = min( last - first, 8 - y );
		if ( first == 0 )
			height = 1; // needs to run hblank after first line, in case x scroll changes
		if ( y == 0 && height == 8 )
			draw_bg_unclipped( *this, first, y, height );
		else
			draw_bg_clipped( *this, first, y, height );
		
		run_hblank( height );
		first += height;
	}
}

#include BLARGG_ENABLE_OPTIMIZER

// Tile cache

inline unsigned long reorder( unsigned long n )
{
	n |= n << 7;
	return ((n << 14) | n);
}

void Nes_Ppu_Impl::update_tile( int index )
{
	const byte* in = chr_rom + (index) * bytes_per_tile;
	byte* out = (byte*) tile_cache [index];
	byte* flipped_out = (byte*) flipped_tiles [index];
	
	uint32_t bit_mask = 0x11111111;
	
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

void Nes_Ppu_Impl::update_tiles( int first_tile )
{
	for ( int i = 0; i < chr_tile_count / modified_chunk_size; i++ )
	{
		unsigned long modified = modified_tiles [i];
		if ( modified )
		{
			modified_tiles [i] = 0;
			
			for ( int bit = 0; bit < modified_chunk_size; bit++ )
			{
				if ( modified & (1 << bit) )
					update_tile( first_tile + i * modified_chunk_size + bit );
			}
		}
	}
}

