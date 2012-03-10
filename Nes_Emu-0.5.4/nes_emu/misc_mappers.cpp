
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

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

// Nina-1

class Mapper_Nina1 : public Nes_Mapper {
	byte bank;
public:
	Mapper_Nina1()
	{
		register_state( &bank, 1 );
	}
	
	void reset()
	{
		Nes_Mapper::reset();
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
		set_prg_bank( 0x8000, bank_32k, bank );
	}
};

// GNROM

class Mapper_Gnrom : public Nes_Mapper {
	byte bank;
public:
	Mapper_Gnrom()
	{
		register_state( &bank, 1 );
	}
	
	void reset()
	{
		Nes_Mapper::reset();
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
			set_prg_bank( 0x8000, bank_32k, (bank >> 4) & 3 );
	}
};

// Camerica

class Mapper_Camerica : public Nes_Mapper {
	byte regs [3];
public:
	Mapper_Camerica()
	{
		register_state( regs, sizeof regs );
	}
	
	void reset()
	{
		Nes_Mapper::reset();
		regs [0] = 0;
		regs [1] = 0;
		regs [2] = 0;
		set_prg_bank( 0xC000, bank_16k, last_bank );
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
			set_prg_bank( 0x8000, bank_16k, data );
		}
		else if ( (addr & 0xf000) == 0x9000 )
		{
			regs [1] = data;
			regs [2] = true;
			mirror_single( (data >> 4) & 1 );
		}
	}
};

void register_misc_mappers();
void register_misc_mappers()
{
	register_mapper<Mapper_Nina1>( 34 );
	register_mapper<Mapper_Gnrom>( 66 );
	register_mapper<Mapper_Camerica>( 71 );
}

