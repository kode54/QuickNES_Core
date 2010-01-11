
// Nes_Emu 0.5.6. http://www.slack.net/~ant/

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

bool const add_wait_states2 = 1;
bool const debug_irq = false; // run one instruction at a time to verify IRQ handling

const int unmapped_fill = 0xf2; // unmapped pages are filled with illegal instruction

enum { low_ram_size = 0x800 };
enum { low_ram_end = 0x2000 };

// Constants are manually duplicated in Nes_Emu so their value can be seen directly
// in Nes_Emu.h, rather than having to look in Nes_Ppu.h.
BOOST_STATIC_ASSERT( Nes_Emu::palette_start == Nes_Ppu::palette_size );
BOOST_STATIC_ASSERT( Nes_Emu::image_width   == Nes_Ppu::image_width );
BOOST_STATIC_ASSERT( Nes_Emu::image_height  == Nes_Ppu::image_height );

static BOOST::uint8_t unmapped_sram [0x2000];

Nes_Emu::Nes_Emu()
{
	rom = NULL;
	impl = NULL;
	mapper = NULL;
	cpu.set_emu( this );
	memset( &nes, 0, sizeof nes );
	memset( &joypad, 0, sizeof joypad );
}

blargg_err_t Nes_Emu::init()
{
	if ( !impl )
	{
		impl = BLARGG_NEW impl_t;
		BLARGG_CHECK_ALLOC( impl );
		impl->apu.dmc_reader( read_dmc, this );
		impl->apu.irq_notifier( apu_irq_changed, this );
	}
	
	return blargg_success;
}

blargg_err_t Nes_Emu::open_rom( Nes_Rom const* new_rom )
{
	close_rom();
	
	BLARGG_RETURN_ERR( init() );
	
	mapper = Nes_Mapper::create( new_rom, this );
	if ( !mapper )
		return "Unsupported mapper";
	
	BLARGG_RETURN_ERR( ppu.open_chr( new_rom->chr(), new_rom->chr_size() ) );
	
	rom = new_rom;
	
	ppu.mapper = mapper;
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
		if ( rom )
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

void Nes_Emu::save_snapshot( Nes_Snapshot* out ) const
{
	out->clear();
	
	out->nes = nes;
	out->nes_valid = true;
	
	out->cpu = cpu.r;
	out->cpu_valid = true;
	
	out->joypad = joypad;
	out->joypad_valid = true;
	
	impl->apu.save_snapshot( &out->apu );
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
	
	out->mapper.size = 0;
	mapper->save_state( out->mapper );
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
		impl->apu.load_snapshot( in.apu );
	
	ppu.load_state( in );
	
	if ( in.ram_valid )
		memcpy( cpu.low_mem, in.ram, sizeof in.ram );
	
	if ( in.sram_size )
	{
		sram_ever_enabled = true;
		memcpy( impl->sram, in.sram, in.sram_size );
		enable_sram( true ); // mapper can override (read-only, unmapped, etc.)
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
	// to do: enable sram now?
	return in.read( impl->sram, impl->sram_size );
}

void Nes_Emu::enable_sram( bool b, bool read_only )
{
	if ( b )
	{
		sram_ever_enabled = true;
		cpu.map_memory( 0x6000, impl->sram_size, read_sram_,
				(read_only ? write_unmapped : write_sram_) );
		cpu.map_code( 0x6000, impl->sram_size, impl->sram );
	}
	else
	{
		cpu.map_memory( 0x6000, impl->sram_size, read_unmapped, write_unmapped );
		for ( int i = 0; i < impl->sram_size; i += cpu.page_size )
			cpu.map_code( 0x6000 + i, cpu.page_size, impl->unmapped_page );
	}
}

// PPU

int Nes_Emu::read_ppu( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->ppu.read( emu->clock(), addr );
}

void Nes_Emu::write_ppu( Nes_Emu* emu, nes_addr_t addr, int data )
{
	//if ( emu->clock() > 4000 && emu->clock() < 4100 )
	//  dprintf( "%6d %02X->%04X\n", emu->clock(), data, addr );
	emu->ppu.write( emu->clock(), addr, data );
}

// Memory

int Nes_Emu::read_rom( Nes_Emu* emu, nes_addr_t addr )
{
	return *emu->cpu.get_code( addr );
}

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

// Unmapped memory

static void log_unmapped( nes_addr_t addr, int data = -1 )
{
	static nes_addr_t last_addr;
	if ( last_addr != addr )
	{
		last_addr = addr;
		if ( data < 0 )
			dprintf( "read unmapped %04X\n", addr );
		else
			dprintf( "write unmapped %04X <- %02X\n", addr, data );
	}
}

int Nes_Emu::read_unmapped( Nes_Emu* emu, nes_addr_t addr )
{
	if ( addr >= 0x10000 )
		return emu->cpu.read( addr & 0xffff );
	
	#ifndef NDEBUG
		log_unmapped( addr );
	#endif
	
	return addr >> 8; // simulate open bus
}

void Nes_Emu::write_unmapped( Nes_Emu* emu, nes_addr_t addr, int data )
{
	if ( addr >= 0x10000 )
	{
		emu->cpu.write( addr & 0xffff, data );
		return;
	}
	
	#ifndef NDEBUG
		log_unmapped( addr, data );
	#endif
}

// I/O and sound

int Nes_Emu::read_dmc( void* data, nes_addr_t addr )
{
	Nes_Emu* emu = (Nes_Emu*) data;
	if ( add_wait_states2 )
		emu->cpu.time( emu->cpu.time() + 4 );
	return emu->cpu.read( addr );
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
		// if strobe goes low, latch data
		if ( joypad.w4016 & 1 & ~data )
		{
			joypad_read_count_++;
			joypad.joypad_latches [0] = current_joypad [0];
			joypad.joypad_latches [1] = current_joypad [1];
		}
		joypad.w4016 = data;
		return;
	}
	
	// apu
	if ( unsigned (addr - impl->apu.start_addr) <= impl->apu.end_addr - impl->apu.start_addr )
	{
		if ( add_wait_states2 )
			if ( addr == 0x4010 || (addr == 0x4015 && (data & 0x10)) )
				irq_changed();
		impl->apu.write_register( clock(), addr, data );
		return;
	}
	
	#ifndef NDEBUG
		log_unmapped( addr, data );
	#endif
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
		return impl->apu.read_status( clock() );
	
	#ifndef NDEBUG
		log_unmapped( addr );
	#endif
	
	return addr >> 8; // simulate open bus
}

int Nes_Emu::read_io_( Nes_Emu* emu, nes_addr_t addr )
{
	return emu->read_io( addr );
}

// CPU

const int irq_inhibit_mask = 0x04;

nes_addr_t Nes_Emu::read_vector( nes_addr_t addr )
{
	return cpu.read( addr ) + 0x100 * cpu.read( addr + 1 );
}

int Nes_Emu::generic_read( nes_time_t t, nes_addr_t addr )
{
	if ( addr >= 0x8000 )
		return read_rom( this, addr );
	
	if ( addr < 0x2000 )
		return read_mirrored_ram( this, addr );
	
	if ( addr < 0x4000 )
		return read_ppu( this, addr );
	
	if ( addr < 0x5000 )
		return read_io_( this, addr );
	
	// to do: only if sram is enabled
	if ( unsigned (addr - 0x1000) < 0x8000 )
		return read_sram_( this, addr );
	
	return read_unmapped( this, addr );
}

void Nes_Emu::generic_write( nes_time_t t, nes_addr_t addr, int data )
{
	if ( addr < 0x2000 )
		write_mirrored_ram( this, addr, data );
	
	if ( addr < 0x4000 )
		write_ppu( this, addr, data );
	
	if ( addr < 0x5000 )
		write_io_( this, addr, data );
	
	// to do: only if sram is write-enabled
	if ( unsigned (addr - 0x1000) < 0x8000 )
		write_sram_( this, addr, data );
	
	write_unmapped( this, addr, data );
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
		cpu.map_memory( 0x2000, 0x2000, read_ppu, write_ppu );
		
		// I/O and sound
		cpu.map_memory( 0x4000, 0x1000, read_io_, write_io_ );
		
		// SRAM
		sram_ever_enabled = false;
		enable_sram( false );
		if ( !rom->has_battery_ram() || erase_battery_ram )
			memset( impl->sram, 0xff, impl->sram_size );
		
		// ROM
		cpu.map_memory( 0x8000, 0x8000, read_rom, write_unmapped );
		
		joypad.joypad_latches [0] = 0;
		joypad.joypad_latches [1] = 0;
		
		nes.frame_count = 0;
	}
	
	// to do: emulate partial reset
	
	ppu.reset( full_reset );
	impl->apu.reset();
	
	mapper->reset();
	
	cpu.r.pc = read_vector( 0xfffc );
	cpu.r.sp = 0xfd;
	cpu.r.a = 0;
	cpu.r.x = 0;
	cpu.r.y = 0;
	cpu.r.status = irq_inhibit_mask;
	//nes.timestamp = 2396;
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
	
	nes.frame_count++;
	joypad_read_count_ = 0;
	current_joypad [0] = joypad1;
	current_joypad [1] = joypad2;
	
	// offset cpu starting time by however far it went over on previous frame
	cpu.time( nes.timestamp );
	nes.timestamp = 0;
	
	nes_time_t nmi_time = ppu.begin_frame();
	while ( true )
	{
		if ( add_wait_states2 )
			impl->apu.run_until( cpu_time() );
		
		// time of next event when cpu needs to stop
		nes_time_t next_event = ppu.frame_length();
		nes_time_t present = cpu_time();
		if ( present >= next_event )
		{
			sync_ppu(); // frame length is adjusted part-way through ppu frame
			next_event = ppu.frame_length();
			if ( present >= next_event )
				break;
		}
		
		if ( debug_irq )
		{
			// run only one instruction at a time
			next_event = present + 1;
			mapper->run_until( present );
		}
		
		// DMC
		if ( add_wait_states2 )
			next_event = min( next_event, impl->apu.next_dmc_read_time() + 1 );
		
		// NMI
		if ( present >= nmi_time )
		{
			nmi_time = LONG_MAX / 2; // to do: common constant
			vector_interrupt( 0xfffa );
		}
		next_event = min( next_event, nmi_time );
		
		// IRQ
		if ( !(cpu.r.status & irq_inhibit_mask) )
		{
			nes_time_t irq = impl->apu.earliest_irq();
			irq = min( irq, mapper->next_irq( present ) );
			
			if ( irq <= present )
			{
				//dprintf( "%6d IRQ vectored\n", present );
				mapper->run_until( present );
				vector_interrupt( 0xfffe );
			}
			else
			{
				next_event = min( next_event, irq );
			}
		}
		
		// CPU
		#ifdef NES_EMU_CPU_HOOK
			Nes_Cpu::result_t result = NES_EMU_CPU_HOOK( cpu, next_event );
		#else
			Nes_Cpu::result_t result = cpu.run( next_event );
		#endif
		
		if ( result == Nes_Cpu::result_badop )
		{
			// treat undefined instructions as nop
			cpu.r.pc++;
			error_count_++;
			cpu.time( cpu.time() + 2 );
		}
	}
	
	nes_time_t total = cpu_time();
	impl->apu.run_until( total );
	check( cpu.time() == total ); // apu dmc wait-states should match predicted count
	
	nes_time_t frame = ppu.frame_length();
	nes.timestamp = total - frame;
	
	// end frames
	impl->apu.end_frame( frame );
	mapper->end_frame( frame );
	ppu.end_frame( frame );
	
	start_frame(); // also prevents any ppu sync calls from running ppu, which would be bad
	
	return frame;
}

