
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

#include <string.h>
#include "Nes_Emu.h"

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

Nes_Mapper::Nes_Mapper()
{
	emu_ = NULL;
	state = NULL;
	state_size = 0;
}

Nes_Mapper::~Nes_Mapper()
{
}

void Nes_Mapper::reset()
{
	int mirroring = rom->mirroring();
	if ( mirroring & 8 )
		mirror_full();
	else if ( mirroring & 1 )
		mirror_vert();
	else
		mirror_horiz();
	
	set_chr_bank( 0, bank_8k, 0 );
	set_prg_bank( 0x8000, bank_16k, 0 );
	set_prg_bank( 0xC000, bank_16k, last_bank );
	memset( state, 0, state_size );
	intercept_writes( 0x8000, 0x8000 );
}

int Nes_Mapper::save_state( mapper_state_t* out )
{
	memcpy( out->state, state, state_size );
	return state_size;
}

void Nes_Mapper::load_state( mapper_state_t const& in )
{
	Nes_Mapper::reset(); // to do: should this call derived reset()?
	memcpy( state, in.state, state_size );
	apply_mapping();
}

// Timing

void Nes_Mapper::irq_changed() { emu_->irq_changed(); }
	
nes_time_t Nes_Mapper::next_irq( nes_time_t ) { return no_irq; }

void Nes_Mapper::a12_clocked( nes_time_t ) { }

void Nes_Mapper::run_until( nes_time_t ) { }

void Nes_Mapper::end_frame( nes_time_t ) { }

bool Nes_Mapper::ppu_enabled() const { return emu().get_ppu().bg_enabled(); }

// Sound

int Nes_Mapper::channel_count() const { return 0; }

void Nes_Mapper::set_channel_buf( int index, Blip_Buffer* ) { require( false ); }

void Nes_Mapper::set_treble( blip_eq_t const& ) { }

// Memory interception

int Nes_Mapper::read_mapper( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->mapper->read( emu->clock(), addr );
}

void Nes_Mapper::write_mapper( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->mapper->write( emu->clock(), addr, data );
}

int Nes_Mapper::read( nes_time_t time, nes_addr_t addr )
{
	return emu().generic_read( time, addr );
}

void Nes_Mapper::write( nes_time_t time, nes_addr_t addr, int data )
{
	emu().generic_write( time, addr, data );
}

void Nes_Mapper::intercept_reads( nes_addr_t addr, unsigned size )
{
	require( addr >= 0x2000 );
	require( addr + size <= 0x10000 );
	nes_addr_t end = addr + size + Nes_Cpu::page_size;
	end  -= end  % Nes_Cpu::page_size;
	addr -= addr % Nes_Cpu::page_size;
	emu().cpu.set_reader( addr, end - addr, read_mapper );
}

void Nes_Mapper::intercept_writes( nes_addr_t addr, unsigned size )
{
	require( addr >= 0x2000 );
	require( addr + size <= 0x10000 );
	nes_addr_t end = addr + size + Nes_Cpu::page_size;
	end  -= end  % Nes_Cpu::page_size;
	addr -= addr % Nes_Cpu::page_size;
	emu().cpu.set_writer( addr, end - addr, write_mapper );
}

// Memory mapping

void Nes_Mapper::enable_sram( bool enabled, bool read_only )
{
	emu_->enable_sram( enabled, read_only );
}

void Nes_Mapper::set_prg_bank( nes_addr_t addr, bank_size_t bs, int bank )
{
	require( addr >= 0x2000 ); // can't remap low-memory
	
	int bank_size = 1 << bs;
	require( addr % bank_size == 0 ); // must be aligned
	
	int bank_count = rom->prg_size() >> bs;
	if ( bank < 0 )
		bank += bank_count;
	
	if ( bank >= bank_count )
	{
		check( !(rom->prg_size() & (rom->prg_size() - 1)) ); // ensure PRG size is power of 2
		bank %= bank_count;
	}
	
	emu().cpu.map_code( addr, bank_size, rom->prg() + (bank << bs) );

	if ( unsigned (addr - 0x6000) < 0x2000 )
		emu().cpu.map_memory( addr, bank_size, Nes_Emu::read_rom, Nes_Emu::write_unmapped );
}

void Nes_Mapper::set_chr_bank( nes_addr_t addr, bank_size_t bs, int bank )
{
	emu().sync_ppu(); 
	emu().get_ppu().set_chr_bank( addr, 1 << bs, bank << bs );
}

void Nes_Mapper::mirror_manual( int page0, int page1, int page2, int page3 )
{
	emu().sync_ppu(); 
	emu().get_ppu().set_nt_banks( page0, page1, page2, page3 );
}

#ifndef NDEBUG
int Nes_Mapper::handle_bus_conflict( nes_addr_t addr, int data )
{
	if ( emu().cpu.get_code( addr ) [0] != data )
		dprintf( "Mapper write had bus conflict\n" );
	return data;
}
#endif

// Mapper registration

int const max_mappers = 32;
Nes_Mapper::mapping_t Nes_Mapper::mappers [max_mappers] =
{
	{ 0, Nes_Mapper::make_nrom },
	{ 1, Nes_Mapper::make_mmc1 },
	{ 2, Nes_Mapper::make_unrom },
	{ 3, Nes_Mapper::make_cnrom },
	{ 4, Nes_Mapper::make_mmc3 },
	{ 7, Nes_Mapper::make_aorom }
};
static int mapper_count = 6; // to do: keep synchronized with pre-supplied mappers above

Nes_Mapper::creator_func_t Nes_Mapper::get_mapper_creator( int code )
{
	for ( int i = 0; i < mapper_count; i++ )
	{
		if ( mappers [i].code == code )
			return mappers [i].func;
	}
	return NULL;
}

void Nes_Mapper::register_mapper( int code, creator_func_t func )
{
	// Catch attempted registration of a different creation function for same mapper code
	require( !get_mapper_creator( code ) || get_mapper_creator( code ) == func );
	require( mapper_count < max_mappers ); // fixed liming on number of registered mappers
	
	mapping_t& m = mappers [mapper_count++];
	m.code = code;
	m.func = func;
}

Nes_Mapper* Nes_Mapper::create( Nes_Rom const* rom, Nes_Emu* emu )
{
	Nes_Mapper::creator_func_t func = get_mapper_creator( rom->mapper_code() );
	if ( !func )
		return NULL;
	
	// to do: out of memory will be reported as unsupported mapper
	Nes_Mapper* mapper = func();
	if ( mapper )
	{
		mapper->rom = rom;
		mapper->emu_ = emu;
	}
	return mapper;
}

