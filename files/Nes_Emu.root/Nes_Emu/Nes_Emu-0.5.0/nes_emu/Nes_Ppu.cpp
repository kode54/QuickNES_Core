
// NES PPU setup

// Nes_Emu 0.5.0. http://www.slack.net/~ant/

#include "Nes_Ppu.h"

#include <string.h>
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
int const chr_addr_size = 0x2000;

Nes_Ppu::Nes_Ppu( int first_host_palette_entry )
{
	require( first_host_palette_entry % palette_size == 0 );
	require( (unsigned) first_host_palette_entry < 0x100 );
	palette_offset = first_host_palette_entry * 0x01010101;
	
	impl = NULL;
	chr_rom = NULL;
	chr_size = 0;
	tile_cache = NULL;
	tile_cache_mem = NULL;
	set_max_sprites( 8 );
	set_pixels( NULL, 0 );
	memset( unused, 0, sizeof unused );
}

Nes_Ppu::~Nes_Ppu()
{
	close_chr();
	delete impl;
}

void Nes_Ppu::close_chr()
{
	delete [] tile_cache_mem;
	tile_cache_mem = NULL;
}

void Nes_Ppu::set_pixels( void* p, long rb )
{
	base_pixels = NULL;
	row_bytes = rb;
	if ( p )
		base_pixels = (byte*) p + rb * image_top + image_left;
}

blargg_err_t Nes_Ppu::open_chr( byte const* new_chr, long chr_rom_size )
{
	close_chr();
	
	if ( !impl )
	{
		impl = BLARGG_NEW impl_t;
		BLARGG_CHECK_ALLOC( impl );
	}
	
	chr_rom = new_chr;
	chr_size = chr_rom_size;
	chr_write_mask = 0; // pre-specified tiles are read-only
	
	if ( chr_rom_size == 0 )
	{
		// CHR RAM
		chr_rom = impl->chr_ram;
		chr_size = sizeof impl->chr_ram;
		memset( impl->chr_ram, 0, sizeof impl->chr_ram );
		chr_write_mask = ~0; // tile ram is read/write
	}
	
	assert( chr_size % chr_addr_size == 0 );
	long tile_count = chr_size / bytes_per_tile;
	tile_cache_mem = BLARGG_NEW byte [tile_count * sizeof (cached_tile_t) * 2 + cache_line_size];
	tile_cache = (cached_tile_t*) (tile_cache_mem + cache_line_size -
			(unsigned long) tile_cache_mem % cache_line_size);
	flipped_tiles = tile_cache + tile_count;
	
	// update entire tile cache
	for ( long pos = 0; pos < chr_size; pos += chr_addr_size )
	{
		memset( tiles_modified, ~0, sizeof tiles_modified );
		update_tiles( pos / bytes_per_tile );
	}
	
	any_tiles_modified = false;
	
	set_chr_bank( 0, chr_addr_size, 0 );
	
	return blargg_success;
}

static const unsigned char initial_palette [0x20] =
{
	0x0f,0x01,0x00,0x01,0x00,0x02,0x02,0x0D,
	0x08,0x10,0x08,0x24,0x00,0x00,0x04,0x2C,
	0x00,0x01,0x34,0x03,0x00,0x04,0x00,0x14,
	0x00,0x3A,0x00,0x02,0x00,0x20,0x2C,0x08
};

void Nes_Ppu::reset( bool full_reset )
{
	w2000 = 0;
	w2001 = 0;
	r2002 = 0x80; // VBL flag immediately set at power-up, but about 27400 clocks after reset
	w2003 = 0;
	r2007 = 0;
	second_write = false;
	vram_addr = 0;
	vram_temp = 0;
	pixel_x = 0;
	
	if ( full_reset )
	{
		memcpy( palette, initial_palette, sizeof palette );
		set_nt_banks( 0, 0, 0, 0 );
		memset( spr_ram, 0, sizeof spr_ram );
		memset( impl->nt_ram, 0, sizeof impl->nt_ram );
		memset( host_palette, 0, sizeof host_palette );
	}
	
	start_frame();
}

void Nes_Ppu::dma_sprites( void const* in )
{
	//if ( w2003 != 0 )
	//  dprintf( "Sprite DMA when $2003 = %02X\n", (int) w2003 );
	
	memcpy( spr_ram + w2003, in, 0x100 - w2003 );
	memcpy( spr_ram, (char*) in + 0x100 - w2003, w2003 );
}

void Nes_Ppu::save_state( Nes_Snapshot* out ) const
{
	out->ppu = *this;
	out->ppu_valid = true;
	
	memcpy( out->spr_ram, spr_ram, sizeof out->spr_ram );
	out->spr_ram_valid = true;
	
	out->nametable_size = (nt_banks_ [3] == 3 ? 0x1000 : 0x800);
	memcpy( out->nametable, impl->nt_ram, out->nametable_size );
	
	out->chr_size = 0;
	if ( chr_write_mask )
	{
		out->chr_size = chr_size;
		assert( out->chr_size <= sizeof out->chr);
		memcpy( out->chr, impl->chr_ram, out->chr_size );
	}
}

void Nes_Ppu::load_state( Nes_Snapshot const& in )
{
	// lightweight reset
	set_nt_banks( 0, 0, 0, 0 );
	set_chr_bank( 0, 0x2000, 0 );
	start_frame();
	
	if ( in.ppu_valid )
		STATIC_CAST(ppu_state_t&) (*this) = in.ppu;
	
	if ( in.spr_ram_valid )
		memcpy( spr_ram, in.spr_ram, sizeof spr_ram );
	
	assert( in.nametable_size <= sizeof impl->nt_ram );
	memcpy( impl->nt_ram, in.nametable, in.nametable_size );
	
	if ( chr_write_mask && in.chr_size )
	{
		assert( in.chr_size <= sizeof impl->chr_ram );
		memcpy( impl->chr_ram, in.chr, in.chr_size );
		any_tiles_modified = true;
		memset( tiles_modified, ~0, sizeof tiles_modified );
	}
}

void Nes_Ppu::set_chr_bank( int addr, int size, long data )
{
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

