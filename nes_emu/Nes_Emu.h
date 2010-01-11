
// Nintendo Entertainment System (NES) emulator

// Nes_Emu 0.5.0. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_EMU_H
#define NES_EMU_H

#include "Nes_Rom.h"
#include "Nes_Apu.h"
#include "Nes_Cpu.h"
#include "Nes_Ppu.h"
class Nes_Mapper;

typedef long frame_count_t; // count of emulated video frames

class Nes_Emu {
public:
	// Optionally specify the first host palette entry to use for graphics rendering.
	// Defaults to a non-zero value to leave room for entries at the beginning usually
	// reserved by the host operating system. Must be a multiple of palette_size.
	enum { palette_start = 64 };
	explicit Nes_Emu( int first_host_palette_entry = palette_start );
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
	enum { palette_size = Nes_Ppu::palette_size }; // number of entries used in host palette
	enum { color_table_size = 64 }; // size of NES color table
	int palette_entry( int ) const;
	
// Additional optional features

	// Access to sound chips, for use with Blip_Buffer
	enum { sound_clock_rate = 1789773 }; // clock rate that sound chips run at
	Nes_Apu& get_apu();
	class Nes_Vrc6* get_vrc6(); // NULL if ROM doesn't use Konami VRC6 sound
	
	// Emulate powering NES off and then back on. If full_reset is false, emulates
	// pressing the reset button only, which doesn't affect memory. If
	// erase_battery_ram is false, preserves current battery RAM (if ROM uses it).
	void reset( bool full_reset = true, bool erase_battery_ram = false );
	
	// Number of undefined CPU instructions encountered. Cleared on reset() and
	// load_snapshot(). A non-zero value indicates that ROM is probably incompatible.
	int error_count() const;
	
	// Alter timestamp
	void set_timestamp( frame_count_t );
	
	// Number of frames emulated since reset. Preserved in snapshots.
	frame_count_t timestamp() const;
	
	// True if joypads were strobed during the last emulated frame
	bool were_joypads_read() const;
	
	// Save snapshot of current emulation state
	void save_snapshot( Nes_Snapshot* ) const;
	
	// Restore snapshot
	void load_snapshot( Nes_Snapshot const& );
	
	// Save/load current battery RAM (used in battery-backed games)
	blargg_err_t save_battery_ram( Data_Writer& );
	blargg_err_t load_battery_ram( Data_Reader& );
	// errors: read/write error
	
	// Free any memory and close ROM, if any was currently open. A new ROM must be
	// loaded before further emulation can take place.
	void close_rom();
	
	// Set sprite mode
	enum sprite_mode_t {
		sprites_hidden = 0,
		sprites_visible = 8,  // limit of 8 sprites per scanline as on NES (default)
		sprites_enhanced = 64 // unlimited sprites per scanline (no flickering)
	};
	void set_sprite_mode( sprite_mode_t );
	
	// End of general interface
public:
	// Mapper use
	void sync_ppu();
	void irq_changed();
	void enable_sram( bool enabled, bool read_only = false );
	Nes_Cpu& get_cpu() { return cpu; }
	Nes_Ppu& get_ppu() { return ppu; }
	Nes_Mapper& get_mapper() { return *mapper; }
	nes_time_t apu_time() const { return apu_time( cpu_time() ); }
	nes_time_t apu_time( nes_time_t t ) const { return t - apu_clock_offset; }
	nes_time_t clock() const { return cpu_time() - 1; } // memory access usually last clock
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
	};
	impl_t* impl; // keep large arrays separate
	
	const char* init_mapper( int code, int mirroring ); // returns name of mapper
	blargg_err_t init_chr();
	blargg_err_t init_chr( Data_Reader&, long size );
	blargg_err_t init_prg( Data_Reader&, long size );
	void start_frame();
	
	// PPU
	Nes_Ppu ppu;
	nes_state_t nes;
	nes_time_t ppu_clock();
	static int read_ppu( Nes_Emu*, nes_addr_t );
	static void write_ppu( Nes_Emu*, nes_addr_t, int data );
	
	// ROM and Mapper
	Nes_Rom const* rom;
	Nes_Mapper* mapper;
	static int read_rom( Nes_Emu*, nes_addr_t );
	static void write_mapper( Nes_Emu*, nes_addr_t, int data );
	
	// APU and Joypad
	int apu_clock_offset;
	joypad_state_t joypad;
	bool joypads_were_strobed;
	unsigned long current_joypad [2];
	void write_io( nes_addr_t, int data );
	int read_io( nes_addr_t );
	static int read_io_( Nes_Emu*, nes_addr_t );
	static void write_io_( Nes_Emu*, nes_addr_t, int data );
	static int read_dmc( void* emu, nes_addr_t );
	static void apu_irq_changed( void* emu );
	
	// CPU
	Nes_Cpu cpu;
	int error_count_;
	nes_addr_t read_vector( nes_addr_t );
	void vector_interrupt( nes_addr_t );
	nes_time_t cpu_time_avail( nes_time_t ) const;
	nes_time_t cpu_time() const;
	static int read_unmapped( Nes_Emu*, nes_addr_t );
	static void write_unmapped( Nes_Emu*, nes_addr_t, int data );
	
	// RAM
	bool sram_ever_enabled;
	void init_memory();
	static int read_ram( Nes_Emu*, nes_addr_t );
	static void write_ram( Nes_Emu*, nes_addr_t, int data );
	static int read_mirrored_ram( Nes_Emu*, nes_addr_t );
	static void write_mirrored_ram( Nes_Emu*, nes_addr_t, int data );
	static int read_unmapped_sram( Nes_Emu*, nes_addr_t );
	static void write_unmapped_sram( Nes_Emu*, nes_addr_t, int data );
	static int read_sram_( Nes_Emu*, nes_addr_t );
	static void write_sram_( Nes_Emu*, nes_addr_t, int data );
};

inline int Nes_Emu::error_count() const { return error_count_; }

inline bool Nes_Emu::were_joypads_read() const { return joypads_were_strobed; }

inline void Nes_Emu::set_timestamp( frame_count_t t ) { nes.frame_count = t; }

inline frame_count_t Nes_Emu::timestamp() const { return nes.frame_count; }

inline void Nes_Emu::set_sprite_mode( sprite_mode_t n ) { ppu.set_max_sprites( n ); }

inline void Nes_Emu::set_pixels( void* p, long n ) { ppu.set_pixels( p, n ); }

inline Nes_Apu& Nes_Emu::get_apu() { return impl->apu; }

inline int Nes_Emu::palette_entry( int i ) const
{
	assert( (unsigned) i < palette_size );
	return ppu.get_palette() [i];
}

#endif

