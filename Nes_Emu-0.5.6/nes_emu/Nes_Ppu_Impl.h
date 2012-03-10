
// NES PPU emulator setup functions

// Nes_Emu 0.5.6. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_PPU_IMPL_H
#define NES_PPU_IMPL_H

#include "Nes_Ppu_Rendering.h"
class Nes_Snapshot;
class Nes_Mapper;
typedef long nes_time_t;

class Nes_Ppu_Impl : public Nes_Ppu_Rendering {
public:
	Nes_Ppu_Impl();
	~Nes_Ppu_Impl();
	
	// Use CHR data and cache optimized transformation of it. Keeps pointer to data.
	// Closes any previous CHR data, even if error occurs.
	blargg_err_t open_chr( const byte*, long size );
	// errors: out of memory
	
	// Free CHR data, if any is loaded
	void close_chr();
	
	// Assign CHR addresses addr to addr + size - 1 to CHR RAM/ROM + data
	void set_chr_bank( int addr, int size, long data );
	
	// Change nametable mapping
	void set_nt_banks( int bank0, int bank1, int bank2, int bank3 );
	
	// Do direct memory copy to sprite RAM
	void dma_sprites( void const* in );
	
	// See Nes_Emu.h
	void set_pixels( void* pixels, long row_bytes );
	void set_sprite_limit( int n ) { sprite_limit = n; }
	
	// Save entire state
	void save_state( Nes_Snapshot* out ) const;
	
	// Host palette
	enum { palette_size = 64 };
	byte host_palette [0x100];
	
	int host_palette_size() const;
	void set_palette_range( int start, int end = 0x100 );
	
	Nes_Mapper* mapper;
	
protected:
	
	byte* caller_pixels;
	long caller_row_bytes;
	enum { vaddr_clock_mask = 0x1000 };
	
	// used by Nes_Ppu
	void reset();
	int read_2007( nes_time_t );
	int write_2007( nes_time_t, int );
	void first_scanline();
	void capture_palette();
	void render_scanlines( int start, int count, byte* pixels, long pitch );
	void run_hblank( int );
	byte const* map_chr( int addr ) const { return &chr_rom [map_chr_addr( addr )]; }
	void load_state( Nes_Snapshot const& );
	
	// I could not think of a shorter name, ugh
	int starting_sprites_per_scanline_count() const { return max_sprites - sprite_limit; }

private:
	
	int host_palette_max;
	int host_palette_size_;
	BOOST::uint32_t base_palette_offset;
	int sprite_limit;
	byte const* chr_rom;
	bool chr_is_writable;
	long chr_size;
	byte* tile_cache_mem;
	bool any_tiles_modified;
	enum { modified_chunk_size = 32 };
	BOOST::uint32_t modified_tiles [chr_tile_count / modified_chunk_size];
	void update_tiles( int first_tile );
	void all_tiles_modified();
	void update_tile( int index );
	static int map_palette( int addr );
	int access_2007( nes_time_t );
	void draw_background( int count );
};

inline int Nes_Ppu_Impl::host_palette_size() const { return host_palette_size_; }

inline void Nes_Ppu_Impl::set_nt_banks( int bank0, int bank1, int bank2, int bank3 )
{
	nt_banks_ [0] = bank0;
	nt_banks_ [1] = bank1;
	nt_banks_ [2] = bank2;
	nt_banks_ [3] = bank3;
}

inline int Nes_Ppu_Impl::map_palette( int addr )
{
	if ( (addr & 3) == 0 )
		addr &= 0x0f; // 0x10, 0x14, 0x18, 0x1c map to 0x00, 0x04, 0x08, 0x0c
	return addr & 0x1f;
}

#include "Nes_Mapper.h"

inline int Nes_Ppu_Impl::access_2007( nes_time_t time )
{
	int addr = vram_addr;
	int old_a12 = ~addr & vaddr_clock_mask;
	int new_addr = addr + (w2000 & 4 ? 32 : 1);
	vram_addr = new_addr;
	if ( new_addr & old_a12 )
		mapper->a12_clocked( time );
	return addr & 0x3fff;
}

inline int Nes_Ppu_Impl::read_2007( nes_time_t time )
{
	int addr = access_2007( time );
	int result = r2007;
	if ( addr < 0x2000 )
	{
		r2007 = *map_chr( addr );
	}
	else
	{
		// read buffer always filled with nametable, even for palette reads
		r2007 = get_nametable( addr ) [addr & 0x3ff];
		if ( addr >= 0x3f00 )
			result = palette [map_palette( addr )];
	}
	return result;
}

inline int Nes_Ppu_Impl::write_2007( nes_time_t time, int data )
{
	int addr = access_2007( time );
	if ( addr < 0x2000 )
	{
		impl->chr_ram [addr] = data; // if CHR is ROM, CHR RAM won't even be used
		any_tiles_modified = chr_is_writable;
		modified_tiles [(unsigned) addr / (bytes_per_tile * modified_chunk_size)] |=
				1 << (((unsigned) addr / bytes_per_tile) % modified_chunk_size);
	}
	else if ( addr < 0x3f00 )
	{
		get_nametable( addr ) [addr & 0x3ff] = data;
	}
	else
	{
		data &= 0x3f;
		byte& entry = palette [map_palette( addr )];
		int old = entry;
		entry = data;
		return old ^ data;
	}
	return 0;
}

inline void Nes_Ppu_Impl::set_palette_range( int start, int end )
{
	assert( start % palette_size == 0 );
	assert( (unsigned) start < 0x100 );
	base_palette_offset = start * 0x01010101;
	host_palette_max = end - start;
}

#endif

