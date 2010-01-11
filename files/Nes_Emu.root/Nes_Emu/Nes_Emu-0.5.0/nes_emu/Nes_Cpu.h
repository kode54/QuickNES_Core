
// Nintendo Entertainment System (NES) 6502 CPU emulator

// Nes_Emu 0.5.0. Copyright (C) 2003-2005 Shay Green. GNU LGPL license.

#ifndef NES_CPU_H
#define NES_CPU_H

#include "blargg_common.h"

	// Nes_Emu-specific options
	// CPU must support IRQ for games to work
	#define NES_CPU_IRQ_SUPPORT 1

	// Keep stack pointer masked to 0xff (some games cause the stack to wrap around)
	#define NES_CPU_MASK_SP 1

typedef long nes_time_t;     // clock cycle count
typedef unsigned nes_addr_t; // 16-bit address

class Nes_Emu;

class Nes_Cpu {
	typedef BOOST::uint8_t uint8_t;
	enum { page_bits = 11 };
	enum { page_count = 0x10000 >> page_bits };
	const uint8_t* code_map [page_count + 1];
public:
	Nes_Cpu();
	
	// Memory read/write function types. Reader must return value from 0 to 255.
	void set_callback_data( Nes_Emu* d ) { callback_data = d; }
	typedef int (*reader_t)( Nes_Emu*, nes_addr_t );
	typedef void (*writer_t)( Nes_Emu*, nes_addr_t, int data );
	
	// Clear registers, unmap memory, and map code pages to unmapped_page.
	void reset( const void* unmapped_page = NULL, reader_t read = NULL, writer_t write = NULL );
	
	// Memory mapping functions take a block of memory of specified 'start' address
	// and 'size' in bytes. Both start address and size must be a multiple of page_size.
	enum { page_size = 1L << page_bits };
	
	// Map code memory (memory accessed via the program counter)
	void map_code( nes_addr_t start, unsigned long size, const void* code );
	
	// Map data memory to read and write functions
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
	// to do: encapsulate registers?
	
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
	#if NES_CPU_IRQ_SUPPORT
		// can only change end_time when IRQ support is enabled
		void end_time( nes_time_t t )       { clock_limit = t - base_time; }
	#endif
	
	// End of public interface
private:
	// noncopyable
	Nes_Cpu( const Nes_Cpu& );
	Nes_Cpu& operator = ( const Nes_Cpu& );
	
	nes_time_t clock_count;
	nes_time_t base_time;
#if NES_CPU_IRQ_SUPPORT
	nes_time_t clock_limit;
#else
	enum { clock_limit = 0 };
#endif

	Nes_Emu* callback_data;
	
	reader_t data_reader [page_count + 1]; // extra entry catches address overflow
	writer_t data_writer [page_count + 1];

public:
	// low_mem is a full page size so it can be mapped with code_map
	uint8_t low_mem [page_size > 0x800 ? page_size : 0x800];
};

inline BOOST::uint8_t* Nes_Cpu::get_code( nes_addr_t addr )
{
	return (uint8_t*) &code_map [addr >> page_bits] [addr & (page_size - 1)];
}
	
inline void Nes_Cpu::end_frame( nes_time_t end_time )
{
	base_time -= end_time;
	assert( time() >= 0 );
}

inline void Nes_Cpu::time( nes_time_t t )
{
	#if NES_CPU_IRQ_SUPPORT
		t -= time();
		clock_limit -= t;
		base_time += t;
	#else
		clock_count = t - base_time;
	#endif
}

inline void Nes_Cpu::push_byte( int data )
{
	int sp = r.sp;
	r.sp = (sp - 1) & 0xff;
	low_mem [0x100 + sp] = data;
}

#endif

