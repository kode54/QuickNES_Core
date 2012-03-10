
// Nes_Emu 0.5.0. http://www.slack.net/~ant/

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

void Nes_Mapper::init( const char* new_name )
{
	emu_ = NULL;
	initial_mirroring = 0;
	chr_is_rom = false;
	name_ = new_name;
	state = NULL;
	state_size = 0;
	prg_size = 0;
	prg = NULL;
	nes_vrc6_ = NULL;
}

Nes_Mapper::Nes_Mapper( const char* new_name )
{
	init( new_name );
}

Nes_Mapper::Nes_Mapper( const char* new_name, void* new_state, int new_state_size )
{
	require( new_state_size <= max_mapper_state_size );
	init( new_name );
	state = new_state;
	state_size = new_state_size;
}

Nes_Mapper::~Nes_Mapper()
{
}

void Nes_Mapper::default_reset()
{
	if ( initial_mirroring & 8 )
		mirror_full();
	else if ( initial_mirroring & 1 )
		mirror_vert();
	else
		mirror_horiz();
	
	set_chr_bank( 0, bank_8k, 0 );
	set_prg_bank( 0, bank_16k, 0 );
	set_prg_bank( 1, bank_16k, last_bank );
	memset( state, 0, state_size );
}

int Nes_Mapper::save_state( mapper_state_t* out )
{
	memcpy( out->state, state, state_size );
	return state_size;
}

void Nes_Mapper::load_state( mapper_state_t const& in )
{
	default_reset();
	memcpy( state, in.state, state_size );
	apply_mapping();
}

void Nes_Mapper::run_until( nes_time_t ) { }

void Nes_Mapper::end_frame( nes_time_t ) { }

nes_time_t Nes_Mapper::next_irq( nes_time_t )
{
	return no_irq;
}

void Nes_Mapper::set_chr_bank( int addr, bank_size_t bs, int bank )
{
	emu()->sync_ppu(); 
	emu()->get_ppu().set_chr_bank( addr, 1 << bs, bank << bs );
}

void Nes_Mapper::set_prg_bank( int cpu_page, bank_size_t bs, int prg_page )
{
	int bank_count = prg_size >> bs;
	if ( prg_page < 0 )
		prg_page += bank_count;
	
	if ( prg_page >= bank_count )
	{
		if ( prg_size & (prg_size - 1) )
			dprintf( "set_prg_bank: bank out of range and PRG not a power of two in size\n" );
		
		prg_page %= bank_count;
	}
	
	emu()->get_cpu().map_code( 0x8000 + (cpu_page << bs), 1 << bs, prg + (prg_page << bs) );
}

void Nes_Mapper::mirror_horiz( int page )
{
	emu()->sync_ppu(); 
	emu()->get_ppu().set_nt_banks( page, page, page ^ 1, page ^ 1 );
}

void Nes_Mapper::mirror_vert( int page )
{
	emu()->sync_ppu(); 
	emu()->get_ppu().set_nt_banks( page, page ^ 1, page, page ^ 1 );
}

void Nes_Mapper::mirror_single( int page )
{
	emu()->sync_ppu(); 
	emu()->get_ppu().set_nt_banks( page, page, page, page );
}

void Nes_Mapper::mirror_full()
{
	emu()->sync_ppu(); 
	emu()->get_ppu().set_nt_banks( 0, 1, 2, 3 );
}

byte const* Nes_Mapper::get_prg() const
{
	return prg;
}

long Nes_Mapper::get_prg_size() const
{
	return prg_size;
}

// None

class Mapper_None : public Nes_Mapper {
public:
	Mapper_None() : Nes_Mapper( "None" ) { }
	
	void reset() { }
	
	void apply_mapping() { }
	
	void write( nes_time_t, nes_addr_t addr, int data ) { }
};

// UNROM

class Mapper_Unrom : public Nes_Mapper {
	byte bank;
public:
	Mapper_Unrom() : Nes_Mapper( "UNROM", &bank, 1 ) { }
	
	void reset()
	{
		bank = 0;
		apply_mapping();
	}
	
	void apply_mapping()
	{
		write( 0, 0, bank );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		bank = data;
		set_prg_bank( 0, bank_16k, bank );
	}
};

// Nina-1

class Mapper_Nina1 : public Nes_Mapper {
	byte bank;
public:
	Mapper_Nina1() : Nes_Mapper( "Nina-1 (Deadly Towers only)", &bank, 1 ) { }
	
	void reset()
	{
		bank = 0;
		apply_mapping();
	}
	
	void apply_mapping()
	{
		write( 0, 0, bank );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		bank = data;
		set_prg_bank( 0, bank_32k, bank );
	}
};

// GNROM

class Mapper_Gnrom : public Nes_Mapper {
	byte bank;
public:
	Mapper_Gnrom() : Nes_Mapper( "GNROM", &bank, 1 ) { }
	
	void reset()
	{
		bank = 0;
		apply_mapping();
	}
	
	void apply_mapping()
	{
		int b = bank;
		bank = ~b;
		write( 0, 0, b );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		int changed = bank ^ data;
		bank = data;
		if ( changed & 0x03 )
			set_chr_bank( 0, bank_8k, bank & 3 );
		if ( changed & 0x30 )
			set_prg_bank( 0, bank_32k, (bank >> 4) & 3 );
	}
};

// AOROM

class Mapper_Aorom : public Nes_Mapper {
	byte bank;
public:
	Mapper_Aorom() : Nes_Mapper( "AOROM", &bank, 1 ) { }
	
	void reset()
	{
		bank = 0;
		apply_mapping();
	}
	
	void apply_mapping()
	{
		int b = bank;
		bank = ~b;
		write( 0, 0, b );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		int changed = bank ^ data;
		bank = data;
		if ( changed & 0x10 )
			mirror_single( (bank >> 4) & 1 );
		
		if ( changed & 0x0f )
			set_prg_bank( 0, bank_32k, bank & 7 );
	}
};
	
// Camerica

class Mapper_Camerica : public Nes_Mapper {
	byte regs [3];
public:
	Mapper_Camerica() : Nes_Mapper( "Camerica", regs, sizeof regs ) { }
	
	void reset()
	{
		regs [0] = 0;
		regs [1] = 0;
		regs [2] = 0;
		set_prg_bank( 1, bank_16k, last_bank );
		apply_mapping();
	}
	
	void apply_mapping()
	{
		write( 0, 0xc000, regs [0] );
		if ( regs [2] )
			write( 0, 0x9000, regs [1] );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		if ( addr >= 0xc000 )
		{
			regs [0] = data;
			set_prg_bank( 0, bank_16k, data );
		}
		else if ( (addr & 0xf000) == 0x9000 )
		{
			regs [1] = data;
			regs [2] = true;
			mirror_single( (data >> 4) & 1 );
		}
	}
};

// CNROM

class Mapper_Cnrom : public Nes_Mapper {
	byte bank;
public:
	Mapper_Cnrom() : Nes_Mapper( "CNROM", &bank, 1 ) { }
	
	void reset()
	{
		bank = 0;
		apply_mapping();
	}
	
	void apply_mapping()
	{
		write( 0, 0, bank );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		bank = data;
		set_chr_bank( 0, bank_8k, bank & 7 );
	}
};

// make_mapper

Nes_Mapper* make_mapper( int code, Nes_Emu* emu )
{
	Nes_Mapper* mapper = NULL;
	
	switch ( code )
	{
		case 0:
			mapper = BLARGG_NEW Mapper_None;
			break;
		
		case 1:
			mapper = Nes_Mapper::make_mmc1();
			break;
		
		case 2:
			mapper = BLARGG_NEW Mapper_Unrom;
			break;
		
		case 3:
			mapper = BLARGG_NEW Mapper_Cnrom;
			break;
		
		case 4:
			mapper = Nes_Mapper::make_mmc3();
			break;
		
		case 5:
			mapper = Nes_Mapper::make_mmc5();
			break;
		
		case 7:
			mapper = BLARGG_NEW Mapper_Aorom;
			break;
		
	#if NES_EMU_ENABLE_VRC6
		case 24:
			mapper = Nes_Mapper::make_vrc6a();
			break;
		
		case 26:
			mapper = Nes_Mapper::make_vrc6b();
			break;
	#endif
	
		case 34:
			mapper = BLARGG_NEW Mapper_Nina1;
			break;
		
		case 66:
			mapper = BLARGG_NEW Mapper_Gnrom;
			break;

		case 69:
			mapper = Nes_Mapper::make_fme07();
			break;
		
		case 71:
			mapper = BLARGG_NEW Mapper_Camerica;
			break;
		
		default:
			return NULL;
	}
	
	// to do: out of memory will be reported as unsupported mapper
	
	mapper->emu_ = emu;
	
	return mapper;
}

