
// NES PPU emulator

// Nes_Emu 0.5.6. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_PPU_H
#define NES_PPU_H

#include "Nes_Ppu_Impl.h"
class Nes_Snapshot;

typedef long nes_time_t;
typedef long ppu_time_t; // ppu_time_t = nes_time_t * 3 (for NTSC NES)

ppu_time_t const ppu_overclock = 3; // PPU clocks for each CPU clock
ppu_time_t const scanline_len = 341;

class Nes_Ppu : public Nes_Ppu_Impl {
	typedef Nes_Ppu_Impl Impl;
public:
	Nes_Ppu();
	~Nes_Ppu();
	
	// See Nes_Ppu_Impl.h and Nes_Ppu_Rendering.h for more interface
	
	void reset( bool full_reset = true );
	
	// Restore entire state
	void load_state( Nes_Snapshot const& );
	
	// True if background rendering is enabled (used by mappers)
	bool bg_enabled() const { return w2001 & 0x08; }
	
	// Begin PPU frame and return time of NMI
	nes_time_t begin_frame();
	
	// Emulate register read/write at given time
	int read( nes_time_t, unsigned addr );
	void write( nes_time_t, unsigned addr, int );
	
	// Number of CPU clocks in current frame
	int frame_length() const { return frame_length_; }
	
	// Synchronization
	void render_until( nes_time_t );
	
	// End frame rendering
	void end_frame( nes_time_t );
	
private:
	
	// to do: make part of state
	byte odd_frame;
	bool nmi_will_occur;
	int extra_clocks;
	
	int frame_length_;
	nes_time_t next_time;
	ppu_time_t scanline_time;
	ppu_time_t hblank_time;
	int scanline_count;
	int frame_phase;
	int query_phase;
	int end_vbl_mask;
	int palette_changed;
	
	void query_until( nes_time_t );
	bool update_sprite_hit( nes_time_t );
	void render_until_( nes_time_t );
	void run_scanlines( int count );
	int read_status( nes_time_t );
	void suspend_rendering();
	ppu_time_t ppu_time( nes_time_t t ) const;
	void write_( nes_time_t, unsigned addr, int );
};

inline void Nes_Ppu::render_until( nes_time_t t )
{
	if ( t >= next_time )
		render_until_( t );
}

inline void Nes_Ppu::write( nes_time_t time, unsigned addr, int data )
{
	if ( addr == 0x2007 ) // most writes are to $2007
	{
		render_until( time );
		if ( write_2007( time, data ) )
			palette_changed = 0x18;
	}
	else
	{
		write_( time, addr, data );
	}
}

#endif

