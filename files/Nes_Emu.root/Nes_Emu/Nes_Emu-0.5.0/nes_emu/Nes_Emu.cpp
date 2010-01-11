
// Nes_Emu 0.5.0. http://www.slack.net/~ant/

#include "Nes_Emu.h"

#include <string.h>
#include "Nes_Mapper.h"
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

bool const add_wait_states = false; // to do: get wait-states working
bool const debug_irq = false; // run one instruction at a time to verify IRQ handling

const int unmapped_fill = 0xf2; // unmapped pages are filled with illegal instruction

enum { low_ram_size = 0x800 };
enum { low_ram_end = 0x2000 };

// Constants are manually duplicated in Nes_Emu so their value can be seen directly
// in Nes_Emu.h, rather than having to look in Nes_Ppu.h.
BOOST_STATIC_ASSERT( Nes_Emu::palette_start == Nes_Emu::palette_size );
BOOST_STATIC_ASSERT( Nes_Emu::image_width   == Nes_Ppu::image_width );
BOOST_STATIC_ASSERT( Nes_Emu::image_height  == Nes_Emu::image_height );

Nes_Emu::Nes_Emu( int first_host_palette_entry ) : ppu( first_host_palette_entry )
{
	rom = NULL;
	impl = NULL;
	mapper = NULL;
	memset( &nes, 0, sizeof nes );
	memset( &joypad, 0, sizeof joypad );
	cpu.set_callback_data( this );
}

blargg_err_t Nes_Emu::open_rom( Nes_Rom const* new_rom )
{
	close_rom();
	
	if ( !impl )
	{
		impl = BLARGG_NEW impl_t;
		BLARGG_CHECK_ALLOC( impl );
		impl->apu.dmc_reader( read_dmc, this );
		impl->apu.irq_notifier( apu_irq_changed, this );
	}
	
	int code = ((new_rom->mapper >> 8) & 0xf0) | ((new_rom->mapper >> 4) & 0x0f);
	mapper = make_mapper( code, this );
	if ( !mapper )
		return "Unsupported mapper";
	mapper->initial_mirroring = new_rom->mapper & 0x09;
	
	mapper->init( this, new_rom->prg(), new_rom->prg_size() );
	
	mapper->chr_is_rom = (new_rom->chr_size() > 0);
	BLARGG_RETURN_ERR( ppu.open_chr( new_rom->chr(), new_rom->chr_size() ) );
	
	rom = new_rom;
	
	memset( impl->unmapped_page, unmapped_fill, sizeof impl->unmapped_page );
	reset( true, true );
	
	return blargg_success;
}

Nes_Emu::~Nes_Emu()
{
	close_rom();
	delete impl;
}

void Nes_Emu::close_rom()
{
	// check that nothing modified unmapped page
	#ifndef NDEBUG
		if ( impl )
		{
			for ( int i = 0; i < sizeof impl->unmapped_page; i++ )
			{
				if ( impl->unmapped_page [i] != unmapped_fill ) {
					dprintf( "Unmapped code page was written to\n" );
					break;
				}
			}
		}
	#endif
	
	rom = NULL;
	delete mapper;
	mapper = NULL;
	
	ppu.close_chr();
}

Nes_Vrc6* Nes_Emu::get_vrc6()
{
	return mapper ? mapper->nes_vrc6_ : NULL;
}

void Nes_Emu::save_snapshot( Nes_Snapshot* out ) const
{
	out->clear();
	
	out->nes = nes;
	out->nes_valid = true;
	
	out->cpu = cpu.r;
	out->cpu_valid = true;
	
	out->joypad = joypad;
	out->joypad_valid = true;
	
	impl->apu.get_state( &out->apu );
	out->apu_valid = true;
	
	ppu.save_state( out );
	
	memcpy( out->ram, cpu.low_mem, sizeof out->ram );
	out->ram_valid = true;
	
	out->sram_size = 0;
	if ( sram_ever_enabled )
	{
		out->sram_size = sizeof impl->sram;
		memcpy( out->sram, impl->sram, out->sram_size );
	}
	
	out->mapper_size = mapper->save_state( &out->mapper );
	out->mapper_valid = true;
}

void Nes_Emu::load_snapshot( Nes_Snapshot const& in )
{
	require( rom );
	
	start_frame();
	error_count_ = 0;
	
	if ( in.nes_valid )
		nes = in.nes;
	
	// always use frame count
	nes.frame_count = in.nes.frame_count;
	if ( nes.frame_count == invalid_frame_count )
		nes.frame_count = 0;
	
	if ( in.cpu_valid )
		cpu.r = in.cpu;
	
	if ( in.joypad_valid )
		joypad = in.joypad;
	
	if ( in.apu_valid )
		impl->apu.set_state( in.apu );
	
	ppu.load_state( in );
	
	if ( in.ram_valid )
		memcpy( cpu.low_mem, in.ram, sizeof in.ram );
	
	if ( in.sram_size )
	{
		sram_ever_enabled = true;
		memcpy( impl->sram, in.sram, in.sram_size );
		enable_sram( true ); // to do: restore properly
	}
	
	if ( in.mapper_valid ) // restore last since it might reconfigure things
		mapper->load_state( in.mapper );
}

blargg_err_t Nes_Emu::save_battery_ram( Data_Writer& out )
{
	return out.write( impl->sram, impl->sram_size );
}

blargg_err_t Nes_Emu::load_battery_ram( Data_Reader& in )
{
	sram_ever_enabled = true;
	return in.read( impl->sram, impl->sram_size );
}

void Nes_Emu::enable_sram( bool b, bool read_only )
{
	if ( b )
	{
		if ( !sram_ever_enabled )
			dprintf( "Enabled SRAM\n" );
		sram_ever_enabled = true;
		cpu.map_memory( 0x6000, impl->sram_size, read_sram_,
				(read_only ? write_unmapped_sram : write_sram_) );
		cpu.map_code( 0x6000, impl->sram_size, impl->sram );
	}
	else
	{
		cpu.map_memory( 0x6000, impl->sram_size, read_unmapped_sram, write_unmapped_sram );
		for ( int i = 0; i < impl->sram_size; i += cpu.page_size )
			cpu.map_code( 0x6000 + i, cpu.page_size, impl->unmapped_page );
	}
}

// Timing

nes_time_t Nes_Emu::cpu_time() const
{
	if ( !add_wait_states )
		return cpu.time();
	
	// to do: offset apu time
	nes_time_t time = cpu.time();
	int count = 0;
	while ( true )
	{
		int new_count = impl->apu.count_dmc_reads( time + count * 4 );
		if ( new_count == count )
			break;
		count = new_count;
	}
	return time + count * 4;
}

void Nes_Emu::irq_changed()
{
	cpu.end_time( 0 );
}

nes_time_t Nes_Emu::cpu_time_avail( nes_time_t end ) const
{
	if ( !add_wait_states )
		return end;
	
	if ( end > 1789773 )
		return end;
	
	// to do: offset apu time
	nes_time_t last_read = end;
	int count = impl->apu.count_dmc_reads( end, &last_read );
	if ( count && end - last_read < 4 ) {
		end = last_read;
		count--;
	}
	
	return end - count * 4;
}

// PPU

// to do: remove
inline nes_time_t Nes_Emu::ppu_clock()
{
	return clock();
}

int Nes_Emu::read_ppu( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->ppu.read( emu->ppu_clock(), addr );
}

void Nes_Emu::write_ppu( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->ppu.write( emu->ppu_clock(), addr, data );
}

void Nes_Emu::sync_ppu()
{
	ppu.render_until( ppu_clock() );
}

// ROM

int Nes_Emu::read_rom( Nes_Emu* emu, nes_addr_t addr )
{
	return *emu->cpu.get_code( addr );
}

void Nes_Emu::write_mapper( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->mapper->write( emu->clock(), addr, data );
}

// RAM

int Nes_Emu::read_ram( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->cpu.low_mem [addr];
}

void Nes_Emu::write_ram( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->cpu.low_mem [addr] = data;
}

int Nes_Emu::read_mirrored_ram( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->cpu.low_mem [addr & (low_ram_size - 1)];
}

void Nes_Emu::write_mirrored_ram( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->cpu.low_mem [addr & (low_ram_size - 1)] = data;
}

int Nes_Emu::read_sram_( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->impl->sram [addr & (impl_t::sram_size - 1)];
}

void Nes_Emu::write_sram_( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->impl->sram [addr & (impl_t::sram_size - 1)] = data;
}

int Nes_Emu::read_unmapped_sram( Nes_Emu* emu, nes_addr_t addr )
{
	if ( !emu->sram_ever_enabled )
		return 0xff;
	dprintf( "Didn't enable SRAM before reading\n" );
	emu->enable_sram( true );
	return read_sram_( emu, addr );
}

void Nes_Emu::write_unmapped_sram( Nes_Emu* emu, nes_addr_t addr, int data )
{
	dprintf( "Didn't enable SRAM before writing\n" );
	emu->enable_sram( true );
	write_sram_( emu, addr, data );
}

// I/O and sound

int Nes_Emu::read_dmc( void* emu, nes_addr_t addr )
{
	Nes_Cpu& cpu = ((Nes_Emu*) emu)->cpu;
	if ( add_wait_states )
	{
		cpu.time( cpu.time() + 4 );
		// CPU end time was reduced to account for DMC wait-states, so re-increase it
		cpu.end_time( cpu.end_time() + 4 );
	}
	return cpu.read( addr );
}

void Nes_Emu::apu_irq_changed( void* emu )
{
	((Nes_Emu*) emu)->irq_changed();
}

inline void Nes_Emu::write_io( nes_addr_t addr, int data )
{
	// sprite dma
	if ( addr == 0x4014 )
	{
		sync_ppu();
		ppu.dma_sprites( cpu.get_code( data * 0x100 ) );
		cpu.time( cpu.time() + 513 );
		return;
	}
	
	// joypad strobe
	if ( addr == 0x4016 )
	{
		//debug_joypad_strobe( data & 1 );
		
		// if strobe goes low, latch data
		if ( joypad.w4016 & 1 & ~data )
		{
			joypads_were_strobed = true;
			joypad.joypad_latches [0] = current_joypad [0];
			joypad.joypad_latches [1] = current_joypad [1];
		}
		joypad.w4016 = data;
		return;
	}
	
	// apu
	if ( unsigned (addr - impl->apu.start_addr) <= impl->apu.end_addr - impl->apu.start_addr )
	{
		//dprintf( "%d %04X <- %02X\n", cpu_time(), addr, data );
		impl->apu.write_register( apu_time(), addr, data );
	}
}

void Nes_Emu::write_io_( Nes_Emu* emu, nes_addr_t addr, int data )
{
	emu->write_io( addr, data );
}

inline int Nes_Emu::read_io( nes_addr_t addr )
{
	if ( (addr & 0xfffe) == 0x4016 )
	{
		// to do: doesn't emulate transparent latch (to aid with movie recording)
		int i = addr & 1;
		//int result = current_joypad [i] & 1;
		int result = joypad.joypad_latches [i] & 1;
		if ( !(joypad.w4016 & 1) )
		{
			//result = joypad.joypad_latches [i] & 1;
			joypad.joypad_latches [i] >>= 1;
		}
		
		//if ( i == 0 ) debug_joypad_read( this );
		return result;
	}
	
	if ( addr == Nes_Apu::status_addr )
		return impl->apu.read_status( apu_time() );
	
	return addr >> 8; // simulate open bus
}

int Nes_Emu::read_io_( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->read_io( addr );
}

// Unmapped memory

int Nes_Emu::read_unmapped( Nes_Emu* emu, nes_addr_t addr )
{
	static nes_addr_t last_addr;
	if ( last_addr != addr )
	{
		last_addr = addr;
		dprintf( "read_unmapped( 0x%04X )\n", (unsigned) addr );
	}
	if ( addr >= 0x10000 )
		return emu->cpu.read( addr & 0xffff );
	
	return addr >> 8; // simulate open bus
}

void Nes_Emu::write_unmapped( Nes_Emu* emu, nes_addr_t addr, int data )
{
	if ( addr >= 0x10000 )
		emu->cpu.write( addr & 0xffff, data );

	static nes_addr_t last_addr;
	if ( last_addr != addr )
	{
		last_addr = addr;
			dprintf( "write_unmapped( 0x%04X, 0x%02X )\n", (unsigned) addr, data );
	}
}

// CPU

const int irq_inhibit_mask = 0x04;

nes_addr_t Nes_Emu::read_vector( nes_addr_t addr )
{
	return cpu.read( addr ) + 0x100 * cpu.read( addr + 1 );
}

void Nes_Emu::start_frame()
{
	apu_clock_offset = 0;
	cpu.time( 0 );
}

void Nes_Emu::reset( bool full_reset, bool erase_battery_ram )
{
	require( rom );
	
	if ( full_reset )
	{
		cpu.reset( impl->unmapped_page, read_unmapped, write_unmapped );
		
		// Low RAM
		memset( cpu.low_mem, 0xff, low_ram_size );
		cpu.low_mem [8] = 0xf7;
		cpu.low_mem [9] = 0xef;
		cpu.low_mem [10] = 0xdf;
		cpu.low_mem [15] = 0xbf;
		cpu.map_memory( 0, low_ram_size, read_ram, write_ram );
		cpu.map_memory( low_ram_size, low_ram_end - low_ram_size,
				read_mirrored_ram, write_mirrored_ram );
		for ( int addr = 0; addr < low_ram_end; addr += low_ram_size )
			cpu.map_code( addr, low_ram_size, cpu.low_mem );
		
		// PPU
		cpu.map_memory( 0x2000, Nes_Cpu::page_size, read_ppu, write_ppu );
		
		// I/O and sound
		cpu.map_memory( 0x4000, 0x1000, read_io_, write_io_ );
		
		// SRAM
		sram_ever_enabled = false;
		enable_sram( false );
		if ( !rom->has_battery_ram() || erase_battery_ram )
			memset( impl->sram, 0xff, impl->sram_size );
		
		// ROM
		cpu.map_memory( 0x8000, 0x8000, read_rom, write_mapper );
		
		joypad.joypad_latches [0] = 0; // to do: only cleared on power-up?
		joypad.joypad_latches [1] = 0;
		
		nes.frame_count = 0;
	}
	
	// to do: emulate partial reset
	
	ppu.reset( full_reset );
	impl->apu.reset();
	
	mapper->default_reset(); 
	mapper->reset();
	
	cpu.r.pc = read_vector( 0xfffc );
	cpu.r.sp = 0xfd;
	cpu.r.a = 0;
	cpu.r.x = 0;
	cpu.r.y = 0;
	cpu.r.status = irq_inhibit_mask;
	nes.timestamp = 0;
	error_count_ = 0;
	
	start_frame();
}

void Nes_Emu::vector_interrupt( nes_addr_t vector )
{
	cpu.push_byte( cpu.r.pc >> 8 );
	cpu.push_byte( cpu.r.pc & 0xff );
	cpu.push_byte( cpu.r.status | 0x20 ); // reserved bit is set
	
	cpu.time( cpu.time() + 7 );
	cpu.r.status |= irq_inhibit_mask;
	cpu.r.pc = read_vector( vector );
}

nes_time_t Nes_Emu::emulate_frame( unsigned long joypad1, unsigned long joypad2 )
{
	require( rom );
	
	joypads_were_strobed = false;
	current_joypad [0] = joypad1;
	current_joypad [1] = joypad2;
	
	// offset cpu starting time by however far it went over on previous frame
	cpu.time( nes.timestamp );
	apu_clock_offset = cpu.time(); // apu needs to start at time 0
	nes.timestamp = 0;
	
	if ( ppu.nmi_enabled() )
		vector_interrupt( 0xfffa );
	
	nes_time_t frame_end = 29780 + (nes.frame_count & 1); // every other frame has extra clock
	nes.frame_count++;

	nes_time_t present;
	while ( (present = cpu_time()) < frame_end )
	{
		// time of next event when cpu needs to stop
		nes_time_t next_event = frame_end;
		if ( debug_irq )
			next_event = present + 1;
		
		// check for irq
		if ( !(cpu.r.status & irq_inhibit_mask) )
		{
			nes_time_t irq = impl->apu.earliest_irq() + apu_clock_offset;
			nes_time_t mirq = mapper->next_irq( present );
			if ( irq > mirq )
				irq = mirq;
			
			if ( irq <= present )
			{
				mapper->run_until( present );
				impl->apu.run_until( apu_time( present ) ); // to do: is this necessary?
				vector_interrupt( 0xfffe );
			}
			else if ( next_event > irq )
			{
				next_event = irq;
			}
		}
		
		// run cpu until time of next event
		nes_time_t cpu_end = cpu_time_avail( next_event );
		#ifdef NES_EMU_CPU_HOOK
			Nes_Cpu::result_t result = NES_EMU_CPU_HOOK( cpu, cpu_end );
		#else
			Nes_Cpu::result_t result = cpu.run( cpu_end );
		#endif
		
		if ( result == Nes_Cpu::result_badop )
		{
			cpu.r.pc++;
			error_count_++;
			cpu.time( cpu.time() + 2 );
		}
	}
	
	// determine how far we went past frame_end
	nes.timestamp = present - frame_end;
	
	// end frames
	impl->apu.end_frame( apu_time( present ) );
	mapper->end_frame( present );
	ppu.end_frame( present );
	
	assert( cpu.time() == present ); // apu dmc wait-states should match predicted count
	
	start_frame();
	
	return present;
}

