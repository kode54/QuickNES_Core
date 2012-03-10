
// NES mapper interface

// Nes_Emu 0.5.4. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_MAPPER
#define NES_MAPPER

#include "Nes_Rom.h"
#include "Nes_Cpu.h"
#include "nes_state.h"
class Blip_Buffer;
class blip_eq_t;

class Nes_Mapper {
public:
	// Register function that creates mapper for given code.
	typedef Nes_Mapper* (*creator_func_t)();
	static void register_mapper( int code, creator_func_t );
	
	// Create mapper appropriate for ROM. Returns NULL if ROM uses unsupported mapper.
	static Nes_Mapper* create( Nes_Rom const*, Nes_Emu* );
	
	virtual ~Nes_Mapper();
	
	// Reset mapper to power-up state. Always called just after mapper is
	// first created. Be sure to call Nes_Mapper::reset() at the beginning
	// of your override.
	virtual void reset();
	
	// Save state. Default saves registered state.
	virtual int save_state( mapper_state_t* );
	
	// Load state. Default loads to registered state, then calls apply_mapping().
	// Mapper must restore previous PRG and CHR mapping.
	virtual void load_state( mapper_state_t const& );
	
// Timing
	
	// Time returned when current mapper state won't ever cause an IRQ
	enum { no_irq = LONG_MAX / 2 };
	
	// Time next IRQ will occur at
	virtual nes_time_t next_irq( nes_time_t present );
	
	// Run mapper until given time
	virtual void run_until( nes_time_t );
	
	// End video frame of given length
	virtual void end_frame( nes_time_t length );
	
// Sound
	
	// Number of sound channels
	virtual int channel_count() const;
	
	// Set sound buffer for channel to output to, or NULL to silence channel.
	virtual void set_channel_buf( int index, Blip_Buffer* );
	
	// Set treble equalization
	virtual void set_treble( blip_eq_t const& );
	
// Misc
	
	// Called when bit 12 of PPU's VRAM address changes from 0 to 1 due to
	// $2006 and $2007 accesses (but not due to PPU scanline rendering).
	virtual void a12_clocked( nes_time_t );
	
protected:
	// Services provided for derived mapper classes
	Nes_Mapper();
	
	// Register state data to automatically save and load. Be sure the binary
	// layout is suitable for use in a file, including any byte-order issues.
	void register_state( void*, int );
	
	// Enable 8K of RAM at 0x6000-0x7FFF, optionally read-only.
	void enable_sram( bool enabled = true, bool read_only = false );
	
	// Cause CPU writes within given address range to call mapper's write() function.
	// Might map a larger address range, which the mapper can ignore and pass to
	// Nes_Mapper::write(). The range 0x8000-0xffff is always intercepted by the mapper.
	void intercept_writes( nes_addr_t addr, unsigned size );
	
	// Cause CPU reads within given address range to call mapper's read() function.
	// Might map a larger address range, which the mapper can ignore and pass to
	// Nes_Mapper::write(). CPU opcode/operand reads and low-memory reads always
	// go directly to memory and cannot be intercepted.
	void intercept_reads( nes_addr_t addr, unsigned size );
	
	// Bank sizes for mapping
	enum bank_size_t { // 1 << bank_Xk = X * 1024
		bank_1k = 10,
		bank_2k = 11,
		bank_4k = 12,
		bank_8k = 13,
		bank_16k = 14,
		bank_32k = 15
	};
	
	// Index of last PRG/CHR bank. Last_bank selects last bank, last_bank - 1
	// selects next-to-last bank, etc.
	enum { last_bank = -1 };
	
	// Map 'size' bytes from 'PRG + bank * size' to CPU address space starting at 'addr'
	void set_prg_bank( nes_addr_t addr, bank_size_t size, int bank );
	
	// Map 'size' bytes from 'CHR + bank * size' to PPU address space starting at 'addr'
	void set_chr_bank( nes_addr_t addr, bank_size_t size, int bank );
	
	// Set PPU mirroring. All mappings implemented using mirror_manual().
	void mirror_manual( int page0, int page1, int page2, int page3 );
	void mirror_single( int page );
	void mirror_horiz( int page = 0 );
	void mirror_vert( int page = 0 );
	void mirror_full();
	
	// True if PPU rendering is enabled. Some mappers watch PPU memory accesses to determine
	// when scanlines occur, and can only do this when rendering is enabled.
	bool ppu_enabled() const;
	
	// True if CHR is ROM. CHR RAM usually isn't bank-switched (and isn't currently
	// supported anyway).
	bool has_chr_rom() const { return rom->chr_size() > 0; }
	
	// True if four-screen mirroring is present
	bool four_screen_mirroring() const { return rom->mirroring() & 0x08; }
	
	// Must be called when next_irq()'s return value is earlier than previous,
	// so end time of current CPU run can be updated. May be called even if time
	// didn't change (or became later), with only minor performance impact.
	void irq_changed();
	
	// Handle data written to mapper that doesn't handle bus conflict arising due to
	// ROM also reading data. Returns data that mapper should act as if were written.
	// Currently always returns 'data' and just checks that data written is the same
	// as byte in ROM at same address and writes debug message if it doesn't
	int handle_bus_conflict( nes_addr_t addr, int data );
	
	// Reference to emulator that uses this mapper.
	Nes_Emu& emu() const { return *emu_; }
	
protected:
	// Services derived classes provide
	
	// Read from memory (optionally overridden in derived class)
	virtual int read( nes_time_t, nes_addr_t );
	
	// Write to memory (overridden in derived class)
	virtual void write( nes_time_t, nes_addr_t, int data );
	
	// Apply current mapping state to hardware. Called after restoring mapper state
	// from a snapshot.
	virtual void apply_mapping() = 0;
	
private:
	Nes_Emu* emu_;
	void* state;
	int state_size;
	Nes_Rom const* rom;
	
	static int read_mapper( Nes_Emu*, nes_addr_t );
	static void write_mapper( Nes_Emu*, nes_addr_t, int data );
	
	struct mapping_t {
		int code;
		Nes_Mapper::creator_func_t func;
	};
	static mapping_t mappers [];
	static creator_func_t get_mapper_creator( int code );
	
	// built-in mappers
	static Nes_Mapper* make_nrom();
	static Nes_Mapper* make_unrom();
	static Nes_Mapper* make_aorom();
	static Nes_Mapper* make_cnrom();
	static Nes_Mapper* make_mmc1();
	static Nes_Mapper* make_mmc3();
};

template<class T>
struct register_mapper {
	/*void*/ register_mapper( int code ) { Nes_Mapper::register_mapper( code, create ); }
	static Nes_Mapper* create() { return BLARGG_NEW T; }
};

#ifdef NDEBUG
inline int Nes_Mapper::handle_bus_conflict( nes_addr_t addr, int data ) { return data; }
#endif

inline void Nes_Mapper::mirror_horiz(  int p ) { mirror_manual( p, p, p ^ 1, p ^ 1 ); }
inline void Nes_Mapper::mirror_vert(   int p ) { mirror_manual( p, p ^ 1, p, p ^ 1 ); }
inline void Nes_Mapper::mirror_single( int p ) { mirror_manual( p, p, p, p ); }
inline void Nes_Mapper::mirror_full()          { mirror_manual( 0, 1, 2, 3 ); }

inline void Nes_Mapper::register_state( void* p, int s )
{
	assert( s <= max_mapper_state_size );
	state = p;
	state_size = s;
}

#endif

