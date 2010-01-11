
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

// to do: time scanline IRQ properly. currently it's quite badly handled.

class Mapper_Mmc3 : public Nes_Mapper, mmc3_state_t {
	nes_time_t irq_time;
	nes_time_t next_time;
public:
	Mapper_Mmc3() : Nes_Mapper( "MMC3",
			STATIC_CAST(mmc3_state_t*) (this), sizeof (mmc3_state_t) ) { }
	
	void update_mirroring();
	void update_chr_banks();
	void update_prg_banks();
	
	void reset()
	{
		memset( banks, 0, sizeof banks );
		mode = 0;
		mirror = 1; // to do: correct?
		sram_mode = 0;
		irq_ctr = 0;
		irq_latch = 0;
		irq_on = 0;
		next_time = 0;
		irq_time = no_irq;
		banks [7] = 1;
		set_prg_bank( 3, bank_8k, last_bank );
		apply_mapping();
	}
	
	void start_frame()
	{
		irq_time = no_irq;
		next_time = 341 * 20 + 280;
	}
	
	void apply_mapping()
	{
		emu()->enable_sram( sram_mode & 0x80, sram_mode & 0x40 );
		update_mirroring();
		update_chr_banks();
		update_prg_banks();
		start_frame();
	}
	
	void run_until( nes_time_t end_time )
	{
		end_time *= 3;
		while ( next_time < end_time )
		{
			if ( irq_ctr-- == 0 )
			{
				irq_ctr = irq_latch;
				irq_time = next_time / 3 + 1;
			}
			next_time += 341;
		}
	}
	
	void end_frame( nes_time_t end_time )
	{
		run_until( end_time );
		start_frame();
	}
	
	nes_time_t next_irq( nes_time_t present )
	{
		if ( !emu()->get_ppu().bg_enabled() )
			return no_irq;
		
		if ( !irq_on || (irq_ctr == 0 && irq_latch == 0) )
			return no_irq;
		
		if ( irq_time + 12 < present )
			irq_time = no_irq;
		
		if ( irq_time != no_irq )
			return irq_time;
		
		return (irq_ctr * 341L + next_time) / 3 + 1;
	}
	
	void write( nes_time_t time, nes_addr_t addr, int data )
	{
		if ( addr & ~0xe001 )
			dprintf( "Wrote to mirrored MMC3 register\n" );
		
		if ( addr >= 0xc000 )
		{
			run_until( time );
			switch ( addr & 0xe001 )
			{
				case 0xc000:
					irq_ctr = data;
					break;
				
				case 0xc001:
					irq_latch = data;
					break;
				
				case 0xe000:
					irq_on = false;
					break;
				
				case 0xe001:
					irq_on = true;
					break;
			}
			emu()->irq_changed();
		}
		else switch ( addr & 0xe001 )
		{
			case 0x8000: {
				int changed = mode ^ data;
				mode = data;
				// avoid unnecessary bank updates
				if ( changed & 0x80 )
					update_chr_banks();
				if ( changed & 0x40 )
					update_prg_banks();
				break;
			}
			
			case 0x8001: {
				int bank = mode & 7;
				banks [bank] = data;
				if ( bank < 6 )
					update_chr_banks();
				else
					update_prg_banks();
				break;
			}
			
			case 0xa000:
				mirror = data;
				update_mirroring();
				break;
			
			case 0xa001:
				sram_mode = data;
				emu()->enable_sram( data & 0x80, data & 0x40 );
				break;
		}
	}
};

void Mapper_Mmc3::update_mirroring()
{
	if ( !(initial_mirroring & 0x08) )
	{
		if ( mirror & 1 )
			mirror_horiz();
		else
			mirror_vert();
	}
}

void Mapper_Mmc3::update_chr_banks()
{
	int chr_xor = (mode & 0x80) << 5;
	set_chr_bank(    0x0 ^ chr_xor, bank_2k, banks [0] >> 1 );
	set_chr_bank(  0x800 ^ chr_xor, bank_2k, banks [1] >> 1 );
	set_chr_bank( 0x1000 ^ chr_xor, bank_1k, banks [2] );
	set_chr_bank( 0x1400 ^ chr_xor, bank_1k, banks [3] );
	set_chr_bank( 0x1800 ^ chr_xor, bank_1k, banks [4] );
	set_chr_bank( 0x1c00 ^ chr_xor, bank_1k, banks [5] );
}

void Mapper_Mmc3::update_prg_banks()
{
	set_prg_bank( 1, bank_8k, banks [7] );
	int bank_swap = (mode & 0x40 ? 2 : 0);
	set_prg_bank( bank_swap ^ 2, bank_8k, last_bank - 1 );
	set_prg_bank( bank_swap, bank_8k, banks [6] );
}

Nes_Mapper* Nes_Mapper::make_mmc3()
{
	return BLARGG_NEW Mapper_Mmc3;
}

