
// Nes_Emu 0.5.0. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

#include <string.h>
#include "Nes_Emu.h"
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

struct fme07_state_t
{
	// written registers
	byte mirroring;
	byte prg_banks [3];
	byte chr_banks [8];
	byte command;
	byte irq_enabled;
	BOOST::uint16_t irq_count;
	byte wram_control;
	
	// internal state
	byte irq_pending;
	BOOST::uint32_t next_time;
};
BOOST_STATIC_ASSERT( sizeof (fme07_state_t) == 24 );

class Mapper_Fme07 : public Nes_Mapper, fme07_state_t {
public:
	Mapper_Fme07() :
			Nes_Mapper( "FME-07", STATIC_CAST(fme07_state_t*) (this), sizeof (fme07_state_t) )
	{
	}
	
	virtual int save_state( mapper_state_t* out )
	{
		set_le16( &irq_count, irq_count );
		set_le32( &next_time, next_time );
		
		return Nes_Mapper::save_state( out );
	}
	
	virtual void load_state( mapper_state_t const& in )
	{
		Nes_Mapper::load_state( in );
		
		irq_count = get_le16( &irq_count );
		next_time = get_le32( &next_time );
	}
	
	void reset()
	{
		memset( chr_banks, 0, sizeof chr_banks );
		
		memset( prg_banks, 0, sizeof prg_banks );

		command = 0;

		wram_control = 0x40;
		
		irq_count = 0xFFFF;
		irq_enabled = false;
		irq_pending = false;
		
		set_prg_bank( 3, bank_8k, last_bank );
		apply_mapping();
	}

	void write_prg_bank( int bank, int data )
	{
		prg_banks [bank] = data;
		set_prg_bank( bank, bank_8k, data );
	}
	
	void write_chr_bank( int bank, int data )
	{
//      dprintf( "change chr bank %d\n", bank );
		chr_banks [bank] = data;
		set_chr_bank( bank * 0x400, bank_1k, data );
	}
	
	void write_mirroring( int data )
	{
		mirroring = data;
		
		if ( data & 2 ) mirror_single( data & 1 );
		else if ( data & 1 ) mirror_horiz();
		else mirror_vert();
	}

	void apply_mapping()
	{
		int i;

		for ( i = 0; i < sizeof prg_banks; ++i )
			write_prg_bank( i, prg_banks [i] );
		
		for ( i = 0; i < sizeof chr_banks; i++ )
			write_chr_bank( i, chr_banks [i] );
		
		write_mirroring( mirroring );

		write_wram_control( wram_control );
	}

	void write_wram_control( int data )
	{
		wram_control = data;

		if ( data & 0x40 )
		{
			emu()->enable_sram( ! ( data & 0x80 ) );
		}
		else
		{
			int bank_count = get_prg_size() >> bank_8k;

			if ( data >= bank_count )
			{
				if ( get_prg_size() & ( get_prg_size() - 1 ) )
					dprintf( "set_prg_bank: bank out of range and PRG not a power of two in size\n" );

				data %= bank_count;
			}

			emu()->get_cpu().map_code( 0x6000, 1 << bank_8k, get_prg() + (data << bank_8k) );
			emu()->get_cpu().map_memory( 0x6000, 1 << bank_8k, read_rom, write_unmapped );
		}
	}

	static int read_rom( Nes_Emu* emu, nes_addr_t addr )
	{
		return *emu->get_cpu().get_code( addr );
	}

	static void write_unmapped( Nes_Emu*, nes_addr_t, int )
	{
	}
	
	void reset_timer( nes_time_t present )
	{
		next_time = present + irq_count;
	}
	
	void run_until( nes_time_t end_time )
	{
		if ( irq_enabled )
		{
			if ( next_time <= end_time )
			{
//              dprintf( "%d timer expired\n", next_time );
				irq_pending = true;
				irq_enabled = false;
				irq_count = 0xFFFF;
			}
			else
			{
				irq_count = next_time - end_time;
			}
		}
	}
	
	void end_frame( nes_time_t end_time )
	{
		run_until( end_time );
		
		next_time -= end_time;
		assert( next_time >= 0 );
	}
	
	nes_time_t next_irq( nes_time_t present )
	{
		if ( irq_pending )
			return present;
		
		if ( irq_enabled )
			return next_time;
		
		return no_irq;
	}
	
	void write( nes_time_t time, nes_addr_t addr, int data )
	{
		if ( addr >= 0xc000 )
		{
			// sound registers
		}
		else if ( addr >= 0xa000 )
		{
			if ( command >= 0x0d )
			{
				// IRQ
				run_until( time );
				//dprintf( "%d FME-07 IRQ [%d] = %02X\n", time, command - 0x0d, data );
				switch ( command )
				{
				case 0x0d:
					irq_enabled = !! data;
					if ( ! data ) irq_pending = false;
					if ( irq_enabled )
						reset_timer( time );
					break;

				case 0x0e:
					irq_count = ( irq_count & 0xFF00 ) | data;
					if ( irq_enabled )
						reset_timer( time );
					break;

				case 0x0f:
					irq_count = ( irq_count & 0x00FF ) | ( data << 8 );
					if ( irq_enabled )
						reset_timer( time );
					break;
				}
				emu()->irq_changed();
			}
			else if ( command == 0x0c )
			{
				write_mirroring( data );
			}
			else if ( command >= 0x09 )
			{
				write_prg_bank( command - 0x09, data );
			}
			else if ( command == 0x08 )
			{
				write_wram_control( data );
			}
			else
			{
				write_chr_bank( command, data );
			}
		}
		else if ( addr >= 0x8000 )
		{
			command = data & 0xf;
		}
	}
};

Nes_Mapper* Nes_Mapper::make_fme07()
{
	return BLARGG_NEW Mapper_Fme07;
}
