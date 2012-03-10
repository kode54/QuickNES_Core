
// Nintendo Entertainment System (NES) 6502 CPU emulator

// Nes_Emu 0.5.4. Copyright (C) 2003-2005 Shay Green. GNU LGPL license.

#ifndef NES_CPU_H
#define NES_CPU_H

#include "blargg_common.h"

typedef long     nes_time_t; // clock cycle count
typedef unsigned nes_addr_t; // 16-bit address

class Nes_Emu;

class Nes_Cpu {
	typedef BOOST::uint8_t uint8_t;
	enum { page_bits = 11 };
	enum { page_count = 0x10000 >> page_bits };
	uint8_t const* code_map [page_count + 1];
public:
	Nes_Cpu();
	
	// Memory read/write function types. Reader must return value from 0 to 255.
	typedef int (*reader_t)( Nes_Emu*, nes_addr_t );
	typedef void (*writer_t)( Nes_Emu*, nes_addr_t, int data );
	void set_emu( Nes_Emu* emu ) { callback_data = emu; }
	
	// Clear registers, unmap memory, and map code pages to unmapped_page.
	void reset( const void* unmapped_page = NULL, reader_t read = NULL, writer_t write = NULL );
	
	// Memory mapping functions take a block of memory of specified 'start' address
	// and 'size' in bytes. Both start address and size must be a multiple of page_size.
	enum { page_size = 1L << page_bits };
	
	// Map code memory (memory accessed via the program counter)
	void map_code( nes_addr_t start, unsigned long size, const void* code );
	
	// Set read function for given address range
	void set_reader( nes_addr_t start, unsigned long size, reader_t );
	
	// Set write function for given address range
	void set_writer( nes_addr_t start, unsigned long size, writer_t );
	
	// Map read and write functions for given address range
	void map_memory( nes_addr_t start, unsigned long size, reader_t, writer_t );
	
	// Access memory as the emulated CPU does.
	int  read( nes_addr_t );
	void write( nes_addr_t, int data );
	uint8_t* get_code( nes_addr_t ); // non-const to allow debugger to modify code
	
	// Push a byte on the stack
	void push_byte( int );
	
	// NES 6502 registers. *Not* kept updated during a call to run().
	struct registers_t {
		BOOST::uint16_t pc;
		BOOST::uint8_t a;
		BOOST::uint8_t x;
		BOOST::uint8_t y;
		BOOST::uint8_t status;
		BOOST::uint8_t sp;
	};
	registers_t r;
	
	// Reasons that run() returns
	enum result_t {
		result_cycles,  // Requested number of cycles (or more) were executed
		result_cli,     // I flag cleared
		result_badop    // unimplemented/illegal instruction
	};
	
	// Run CPU to or after end_time, or until a stop reason from above
	// is encountered. Return the reason for stopping.
	result_t run( nes_time_t end_time );
	
	nes_time_t time() const             { return base_time + clock_count; }
	void time( nes_time_t t );
	nes_time_t end_time() const         { return base_time + clock_limit; }
	void end_frame( nes_time_t );
	void end_time( nes_time_t t )       { clock_limit = t - base_time; }
	
	// End of public interface
private:
	// noncopyable
	Nes_Cpu( const Nes_Cpu& );
	Nes_Cpu& operator = ( const Nes_Cpu& );
	
	nes_time_t clock_count;
	nes_time_t base_time;
	nes_time_t clock_limit;

	Nes_Emu* callback_data;
	
	reader_t data_reader [page_count + 1]; // extra entry catches address overflow
	writer_t data_writer [page_count + 1];
	void set_code_page( int, uint8_t const* );
	
public:
	// low_mem is a full page size so it can be mapped with code_map
	uint8_t low_mem [page_size > 0x800 ? page_size : 0x800];
};

inline BOOST::uint8_t* Nes_Cpu::get_code( nes_addr_t addr )
{
	#if BLARGG_NONPORTABLE
		return (uint8_t*) code_map [addr >> page_bits] + addr;
	#else
		return (uint8_t*) code_map [addr >> page_bits] + (addr & (page_size - 1));
	#endif
}
	
inline void Nes_Cpu::end_frame( nes_time_t end_time )
{
	base_time -= end_time;
	assert( time() >= 0 );
}

inline void Nes_Cpu::time( nes_time_t t )
{
	t -= time();
	clock_limit -= t;
	base_time += t;
}

inline void Nes_Cpu::push_byte( int data )
{
	int sp = r.sp;
	r.sp = (sp - 1) & 0xff;
	low_mem [0x100 + sp] = data;
}

inline void Nes_Cpu::map_memory( nes_addr_t addr, unsigned long s, reader_t r, writer_t w )
{
	set_reader( addr, s, r );
	set_writer( addr, s, w );
}

#endif

