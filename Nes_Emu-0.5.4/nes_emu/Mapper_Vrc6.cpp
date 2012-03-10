
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

#include <string.h>
#include "Nes_Emu.h"
#include "Nes_Vrc6.h"
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

struct vrc6_state_t
{
	// written registers
	byte prg_16k_bank;
	byte old_sound_regs [3] [3]; // to do: eliminate this duplicate
	byte mirroring;
	byte prg_8k_bank;
	byte chr_banks [8];
	byte irq_reload;
	byte irq_mode;
	
	// internal state
	BOOST::uint16_t next_time;
	byte irq_pending;
	byte unused;
	
	vrc6_snapshot_t sound_state;
};
BOOST_STATIC_ASSERT( sizeof (vrc6_state_t) == 26 + sizeof (vrc6_snapshot_t) );

static bool memequal( void const* p, int ch, unsigned long count )
{
	while ( count-- )
	{
		if ( *(byte*) p != ch )
			return false;
		p = (byte*) p + 1;
	}
	return true;
}

class Mapper_Vrc6 : public Nes_Mapper, vrc6_state_t {
	int swap_mask;
	Nes_Vrc6 sound;
	enum { timer_period = 113 * 4 + 3 };
public:
	Mapper_Vrc6( int sm )
	{
		swap_mask = sm;
		vrc6_state_t* state = this;
		register_state( state, sizeof *state );
	}
	
	virtual int channel_count() const { return sound.osc_count; }
	
	virtual void set_channel_buf( int i, Blip_Buffer* b ) { sound.osc_output( i, b ); }
	
	virtual void set_treble( blip_eq_t const& eq ) { sound.treble_eq( eq ); }
	
	virtual int save_state( mapper_state_t* out )
	{
		sound.save_snapshot( &sound_state );
		
		set_le16( &next_time, next_time );
		for ( int i = 0; i < sound.osc_count; i++ )
			set_le16( &sound_state.delays [i], sound_state.delays [i] );
		
		return Nes_Mapper::save_state( out );
	}
	
	virtual void load_state( mapper_state_t const& in )
	{
		Nes_Mapper::load_state( in );
		
		next_time = get_le16( &next_time );
		for ( int i = 0; i < sound.osc_count; i++ )
			sound_state.delays [i] = get_le16( &sound_state.delays [i] );
		
		// to do: eliminate when format is updated
		// old-style registers
		if ( !memequal( old_sound_regs, 0, sizeof old_sound_regs ) )
		{
			dprintf( "Using old VRC6 sound register format\n" );
			memcpy( sound_state.regs, old_sound_regs, sizeof sound_state.regs );
			memset( old_sound_regs, 0, sizeof old_sound_regs );
		}
		
		sound.load_snapshot( sound_state );
	}
	
	virtual void reset()
	{
		Nes_Mapper::reset();
		
		memset( chr_banks, 0, sizeof chr_banks );
		memset( old_sound_regs, 0, sizeof old_sound_regs );
		memset( &sound_state, 0, sizeof sound_state );
		
		prg_16k_bank = 0;
		prg_8k_bank = last_bank - 1;
		
		irq_reload = 0;
		irq_mode = 0;
		irq_pending = false;
		
		next_time = 0;
		sound.reset();
		
		set_prg_bank( 0xE000, bank_8k, last_bank );
		apply_mapping();
	}
	
	void write_chr_bank( int bank, int data )
	{
		//dprintf( "change chr bank %d\n", bank );
		chr_banks [bank] = data;
		set_chr_bank( bank * 0x400, bank_1k, data );
	}
	
	void write_mirroring( int data )
	{
		mirroring = data;
		
		//dprintf( "Change mirroring %d\n", data );
//      emu()->enable_sram( data & 0x80 ); // to do: needed?
		int page = (data >> 5) & 1;
		if ( data & 8 )
			mirror_single( ((data >> 2) ^ page) & 1 );
		else if ( data & 4 )
			mirror_horiz( page );
		else
			mirror_vert( page );
	}

	void apply_mapping()
	{
		set_prg_bank( 0x8000, bank_16k, prg_16k_bank );
		set_prg_bank( 0xC000, bank_8k, prg_8k_bank );
		
		for ( int i = 0; i < sizeof chr_banks; i++ )
			write_chr_bank( i, chr_banks [i] );
		
		write_mirroring( mirroring );
	}
	
	void reset_timer( nes_time_t present )
	{
		next_time = present + unsigned ((0x100 - irq_reload) * timer_period) / 4;
	}
	
	virtual void run_until( nes_time_t end_time )
	{
		if ( irq_mode & 2 )
		{
			while ( next_time < end_time )
			{
				//dprintf( "%d timer expired\n", next_time );
				irq_pending = true;
				reset_timer( next_time );
			}
		}
	}
	
	virtual void end_frame( nes_time_t end_time )
	{
		run_until( end_time );
		
		next_time -= end_time;
		assert( next_time >= 0 );
		
		sound.end_frame( end_time );
	}
	
	virtual nes_time_t next_irq( nes_time_t present )
	{
		if ( irq_pending )
			return present;
		
		if ( irq_mode & 2 )
			return next_time + 1;
		
		return no_irq;
	}
	
	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		if ( (addr + 1) & 2 ) // swap 1 and 2
			addr ^= swap_mask;
		
		if ( addr >= 0xf000 )
		{
			// IRQ
			run_until( time );
			//dprintf( "%d VRC6 IRQ [%d] = %02X\n", time, addr & 3, data );
			switch ( addr & 3 )
			{
				case 0:
					irq_reload = data;
					break;
				
				case 1:
					irq_pending = false;
					irq_mode = data;
					if ( data & 2 )
						reset_timer( time );
					break;
				
				case 2:
					irq_pending = false;
					irq_mode = (irq_mode & ~2) | ((irq_mode << 1) & 2);
					break;
			}
			irq_changed();
		}
		else if ( addr >= 0xd000 )
		{
			write_chr_bank( ((addr >> 11) & 4) | (addr & 3), data );
		}
		else switch ( addr & 0xf003 )
		{
			case 0x8000:
				prg_16k_bank = data;
				set_prg_bank( 0x8000, bank_16k, data );
				break;
			
			case 0xb003:
				write_mirroring( data );
				break;
			
			case 0xc000:
				prg_8k_bank = data;
				set_prg_bank( 0xC000, bank_8k, data );
				break;
			
			default: {
				int osc = unsigned (addr - sound.base_addr) / sound.addr_step;
				int reg = addr & 3;
				if ( (unsigned) osc < sound.osc_count && reg < sound.reg_count )
					sound.write_osc( time, osc, reg, data );
				break;
			}
		}
	}
};

static Nes_Mapper* make_vrc6a()
{
	return BLARGG_NEW Mapper_Vrc6( 0 );
}

static Nes_Mapper* make_vrc6b()
{
	return BLARGG_NEW Mapper_Vrc6( 3 );
}

void register_vrc6_mapper();
void register_vrc6_mapper()
{
	Nes_Mapper::register_mapper( 24, make_vrc6a );
	Nes_Mapper::register_mapper( 26, make_vrc6b );
}

