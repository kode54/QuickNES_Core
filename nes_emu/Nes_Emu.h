
// Nintendo Entertainment System (NES) emulator

// Nes_Emu 0.5.6. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_EMU_H
#define NES_EMU_H

#include "Nes_Rom.h"
#include "Nes_Apu.h"
#include "Nes_Cpu.h"
#include "Nes_Ppu.h"
class Nes_Mapper;

class Nes_Emu {
public:
	Nes_Emu();
	~Nes_Emu();
	
	// Open NES ROM for emulation. Keeps copy of pointer to ROM. Closes previous ROM
	// even if new ROM can't be opened. After opening, ROM's CHR data shouldn't be
	// modified since a copy is cached internally.
	blargg_err_t open_rom( Nes_Rom const* );
	// errors: out of memory
	
	// Graphics are rendered to an 8-bit pixel buffer supplied by the user. The screen
	// image is image_width x image_height pixels, but rendering leaves garbage outside
	// the image so the buffer must be slightly larger, buffer_width x buffer_height
	// pixels. The top-left pixel of the usable image is at image_left, image_top.
	enum { image_width   = 256 };
	enum { image_height  = 240 };
	enum { buffer_width  = Nes_Ppu::buffer_width };
	enum { buffer_height = Nes_Ppu::buffer_height };
	enum { image_left    = Nes_Ppu::image_left };
	enum { image_top     = Nes_Ppu::image_top };
	enum { bits_per_pixel = 8 }; // number of bits per pixel in graphics buffer
	
	// Set graphics buffer to render pixels to. Pixels points to top-left pixel and
	// row_bytes is the number of bytes to get to the next line (positive or negative).
	void set_pixels( void* pixels, long row_bytes );
	
	// Emulate one video frame and return number of clock cycles elapsed,
	// to pass to Blip_Buffer::end_frame(). Joypad1 and joypad2 are the
	// current joypad inputs.
	nes_time_t emulate_frame( unsigned long joypad1, unsigned long joypad2 = 0 );
	
	// Number of frames generated per second
	enum { frames_per_second = 60 };
	
	// Get color that should be used given palette entry of graphics buffer, where 0
	// is the first palette entry used by the emulator, as set by the constructor.
	// Index should be from 0 to palette_size - 1. The returned color is an index into
	// the 64-color NES color table, which the caller must obtain elsewhere (there
	// are a large number of NES palettes available, each with slightly different
	// approximations of the colors as they would appear on a television).
	enum { color_table_size = 64 }; // size of fixed NES color table, not provided here
	enum { max_palette_size = 0x100 };
	int palette_size() const;
	int palette_entry( int ) const;
	
// Additional optional features (can be ignored without any problem)
	
	// Initialize so that APU is available for configuration. Optional; automatically
	// called (if not already) when a ROM is first loaded.
	blargg_err_t init();
	
	// Emulate powering NES off and then back on. If full_reset is false, emulates
	// pressing the reset button only, which doesn't affect memory. If
	// erase_battery_ram is false, preserves current battery RAM (if ROM uses it).
	void reset( bool full_reset = true, bool erase_battery_ram = false );
	
	// Save/load snapshot of emulator state
	void save_snapshot( Nes_Snapshot* ) const;
	void load_snapshot( Nes_Snapshot const& );
	
	// Save/load current battery RAM (used in battery-backed games)
	blargg_err_t save_battery_ram( Data_Writer& );
	blargg_err_t load_battery_ram( Data_Reader& );
	// errors: read/write error
	
	// Free any memory and close ROM, if one was currently open. A new ROM must be
	// loaded before further emulation can take place.
	void close_rom();
	
	// Access to sound chips, for use with Blip_Buffer
	enum { sound_clock_rate = 1789773 }; // clock rate that sound chips run at
	Nes_Apu& get_apu();
	Nes_Mapper& get_mapper() { return *mapper; }
	
	// Hide/show/enhance sprites
	enum sprite_mode_t {
		sprites_hidden = 0,
		sprites_visible = 8,  // limit of 8 sprites per scanline as on NES (default)
		sprites_enhanced = 64 // unlimited sprites per scanline (no flickering)
	};
	void set_sprite_mode( sprite_mode_t );
	
	// Number of undefined CPU instructions encountered. Cleared on reset() and
	// load_snapshot(). A non-zero value indicates that ROM is probably incompatible.
	int error_count() const;
	
	// Number of times joypads were strobed during frame
	int joypad_read_count() const;
	
	// Set range of host palette entries to use for rendering. Defaults to
	// 'palette_start', a a non-zero value to leave room at the beginning for
	// entries usually reserved by the host operating system. Must be a multiple
	// of palette_size.
	enum { palette_start = 64 };
	void set_palette_range( int begin, int end = 0x100 );
	
	// Each pixel in the graphics buffer can be masked with this to yield the only
	// important bits of the palette index. Internally bit 0x20 is used to hold
	// pixel priority information, and palette is duplicated twice to make this
	// bit have no effect on the color.
	enum { palette_mask = 0x1f };
	
// Access to internals, for viewer, cheater, or debugger
	
	// CHR (tiles)
	byte* chr_mem();
	long chr_size() const;
	blargg_err_t chr_mem_changed(); // call if you change CHR data
	
	// Nametable (tile indicies on screen)
	byte* nametable_mem() { return ppu.impl->nt_ram; }
	long nametable_size() const { return 0x1000; }
	
	// Built-in 2K memory
	byte* low_mem() { return cpu.low_mem; }
	long  low_mem_size() const { return 0x800; }
	
	// Optional 8K memory
	byte* high_mem() { return impl->sram; }
	long  high_mem_size() const { return 0x2000; }
	
	// End of general interface
public:
	Nes_Ppu& get_ppu() { return ppu; }
	void irq_changed() { cpu.end_time( 0 ); } // used by debugger to stop cpu
protected:
	// Alter timestamp
	void set_timestamp( long );
	
	// Number of frames emulated since reset. Preserved in snapshots.
	long timestamp() const;
	
private:
	friend class Nes_Mapper;
	
	// Nes_Mapper use
	void sync_ppu();
	void enable_sram( bool enabled, bool read_only = false );
	//Nes_Cpu& get_cpu() { return cpu; }
	nes_time_t clock() const { return cpu_time() - 1; } // memory access usually last clock
	
	int generic_read( nes_time_t, nes_addr_t );
	void generic_write( nes_time_t, nes_addr_t, int data );
	
private:
	
	// noncopyable
	Nes_Emu( const Nes_Emu& );
	Nes_Emu& operator = ( const Nes_Emu& );
	
	struct impl_t
	{
		enum { sram_size = 0x2000 };
		BOOST::uint8_t sram [sram_size];
		Nes_Apu apu;
		BOOST::uint8_t unmapped_page [Nes_Cpu::page_size];
		
		// extra byte allows CPU to always read operand of instruction, which
		// might go past end of ROM
		char extra_byte;
	};
	impl_t* impl; // keep large arrays separate
	
	// Timing
	void start_frame() { cpu.time( 0 ); }
	nes_time_t cpu_time() const { return cpu.time(); }
	
	// PPU
	Nes_Ppu ppu;
	nes_state_t nes;
	static int  read_ppu( Nes_Emu*, nes_addr_t );
	static void write_ppu( Nes_Emu*, nes_addr_t, int data );
	
	// ROM and Mapper
	Nes_Rom const* rom;
	Nes_Mapper* mapper;
	static int  read_rom( Nes_Emu*, nes_addr_t );
	
	// APU and Joypad
	joypad_state_t joypad;
	int joypad_read_count_;
	unsigned long current_joypad [2];
	int  read_io( nes_addr_t );
	void write_io( nes_addr_t, int data );
	static int  read_io_( Nes_Emu*, nes_addr_t );
	static void write_io_( Nes_Emu*, nes_addr_t, int data );
	static int  read_dmc( void* emu, nes_addr_t );
	static void apu_irq_changed( void* emu );
	
	// RAM
	bool sram_ever_enabled;
	void init_memory();
	static int  read_ram( Nes_Emu*, nes_addr_t );
	static void write_ram( Nes_Emu*, nes_addr_t, int data );
	static int  read_mirrored_ram( Nes_Emu*, nes_addr_t );
	static void write_mirrored_ram( Nes_Emu*, nes_addr_t, int data );
	static int  read_unmapped_sram( Nes_Emu*, nes_addr_t );
	static void write_unmapped_sram( Nes_Emu*, nes_addr_t, int data );
	static int  read_sram_( Nes_Emu*, nes_addr_t );
	static void write_sram_( Nes_Emu*, nes_addr_t, int data );
	
	// CPU
	int error_count_;
	Nes_Cpu cpu;
	nes_addr_t read_vector( nes_addr_t );
	void vector_interrupt( nes_addr_t );
	static int  read_unmapped( Nes_Emu*, nes_addr_t );
	static void write_unmapped( Nes_Emu*, nes_addr_t, int data );
};

inline int Nes_Emu::error_count() const { return error_count_; }

inline int Nes_Emu::joypad_read_count() const { return joypad_read_count_; }

inline void Nes_Emu::set_timestamp( long t ) { nes.frame_count = t; }

inline long Nes_Emu::timestamp() const { return nes.frame_count; }

inline void Nes_Emu::set_sprite_mode( sprite_mode_t n ) { ppu.set_sprite_limit( n ); }

inline void Nes_Emu::set_pixels( void* p, long n ) { ppu.set_pixels( p, n ); }

inline Nes_Apu& Nes_Emu::get_apu() { assert( impl ); return impl->apu; }

inline int Nes_Emu::palette_size() const { return ppu.host_palette_size(); }

inline int Nes_Emu::palette_entry( int i ) const
{
	assert( (unsigned) i < palette_size() );
	return ppu.host_palette [i];
}

inline byte* Nes_Emu::chr_mem()
{
	return rom->chr_size() ? (byte*) rom->chr() : ppu.impl->chr_ram;
}

inline long Nes_Emu::chr_size() const
{
	return rom->chr_size() ? rom->chr_size() : ppu.chr_addr_size;
}

inline blargg_err_t Nes_Emu::chr_mem_changed()
{
	return ppu.open_chr( rom->chr(), rom->chr_size() );
}

inline void Nes_Emu::set_palette_range( int i, int j ) { ppu.set_palette_range( i, j ); }

inline void Nes_Emu::sync_ppu() { ppu.render_until( clock() ); }

#endif

