
// NES PPU emulator

// Nes_Emu 0.5.0. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_PPU_H
#define NES_PPU_H

#include "blargg_common.h"
#include "abstract_file.h"
#include "nes_state.h"
class Nes_Snapshot;

typedef long nes_time_t;
typedef long ppu_time_t; // ppu_time_t = nes_time_t * 3 (for NTSC NES)

	// state needed by function templates in Nes_Ppu_Rendering
	struct Nes_Ppu_ : ppu_state_t
	{
		typedef BOOST::uint8_t byte;
		
		// tiles
		enum { bytes_per_tile = 16 };
		typedef BOOST::uint32_t cache_t;
		typedef cache_t cached_tile_t [4];
		cached_tile_t* tile_cache;
		cached_tile_t* flipped_tiles;
		cached_tile_t const& get_tile( int index, bool h_flip = false ) const;
		
		// chr mapping
		enum { chr_page_size = 0x400 };
		enum { chr_addr_size = 0x2000 };
		long chr_pages [chr_addr_size / chr_page_size];
		long map_chr_addr( int ) const;
		
		// nametable mapping
		enum { nt_ram_size = 0x1000 };
		byte nt_banks_ [4];
		byte* get_nametable( int addr );
		
		// output buffer
		byte* base_pixels;
		long row_bytes;
		BOOST::uint32_t palette_offset;
		
		// sprites
		byte sprite_scanlines [256 + 16];
		byte spr_ram [0x100];
		
		struct impl_t
		{
			byte nt_ram [nt_ram_size];
			BOOST::uint32_t clip_buf [256 * 2];
			byte chr_ram [chr_addr_size];
		};
		impl_t* impl; // keep large arrays separate
		
	};

class Nes_Ppu : private Nes_Ppu_ {
public:
	explicit Nes_Ppu( int first_host_palette_entry = 0 );
	~Nes_Ppu();
	
	// Use CHR data and cache optimized transformation of it. Keeps pointer to data.
	// Closes any previous CHR data, even if error occurs.
	blargg_err_t open_chr( const byte*, long size );
	// errors: out of memory
	
	// Free CHR data, if any is loaded
	void close_chr();
	
	// Assign CHR addresses addr to addr + size - 1 to CHR RAM/ROM + data
	void set_chr_bank( int addr, int size, long data );
	
	void reset( bool full_reset = true );
	
	// See Nes_Emu.h
	enum { image_width = 256 };
	enum { image_height = 240 };
	enum { image_top = 0 };
	enum { image_left = 8 };
	enum { buffer_width = image_width + 16 };
	enum { buffer_height = image_height };
	void set_pixels( void* pixels, long row_bytes );
	void set_max_sprites( int n ) { max_sprites = n; }
	
	// Host access to palette
	enum { palette_size = 64 };
	byte const* get_palette() const { return host_palette; }
	
	// Change nametable mapping
	void set_nt_banks( int bank0, int bank1, int bank2, int bank3 );
	
	// Emulate register read/write at given time
	int read( nes_time_t, unsigned addr );
	void write( nes_time_t, unsigned addr, int );
	
	// Do direct memory copy to sprite RAM
	void dma_sprites( void const* in );
	
	// True if NMI is enabled
	bool nmi_enabled() const { return w2000 & 0x80; }
	
	// True if background rendering is enabled
	bool bg_enabled() const { return w2001 & 0x08; }
	
	// Synchronization
	void render_until( nes_time_t );
	void end_frame( nes_time_t );
	
	// Save/restore entire state
	void save_state( Nes_Snapshot* out ) const;
	void load_state( Nes_Snapshot const& );
	
	// End of general interface
private:
	
	int max_sprites;
	byte host_palette [palette_size];
	byte* map_palette( int );
	
	// frame handling
	nes_time_t next_time;
	ppu_time_t scanline_time;
	ppu_time_t hblank_time;
	int next_scanline;
	int frame_phase;
	int query_phase;
	void start_frame();
	void query_until( nes_time_t );
	bool update_sprite_hit( nes_time_t );
	void render_until_( nes_time_t );
	void run_hblank( int );
	
	// low-level rendering
	void draw_background( int begin, int end );
	void black_background( int begin, int end );
	void clip_sprites_left( int begin, int end );
	void clip_left( int begin, int end );
	void save_left( int begin, int end );
	void restore_left( int begin, int end );
	void draw_sprites( int begin, int end );
	
	// CHR data and cache
	byte* tile_cache_mem;
	byte const* chr_rom;
	long chr_size;
	bool any_tiles_modified;
	int chr_write_mask;
	char tiles_modified [0x200];
	void update_tiles( int first_tile );
	byte const* map_chr( int addr ) const;
};

inline void Nes_Ppu::render_until( nes_time_t t )
{
	if ( t >= next_time )
		render_until_( t );
}

inline byte* Nes_Ppu_::get_nametable( int addr )
{
	return &impl->nt_ram [nt_banks_ [(addr >> 10) & 3] * 0x400];
}

inline void Nes_Ppu::set_nt_banks( int bank0, int bank1, int bank2, int bank3 )
{
	nt_banks_ [0] = bank0;
	nt_banks_ [1] = bank1;
	nt_banks_ [2] = bank2;
	nt_banks_ [3] = bank3;
}

inline long Nes_Ppu_::map_chr_addr( int addr ) const
{
	return chr_pages [(unsigned) addr / chr_page_size] + (unsigned) addr % chr_page_size;
}

#endif

