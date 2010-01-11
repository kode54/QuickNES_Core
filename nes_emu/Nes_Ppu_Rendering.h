
// NES PPU emulator graphics rendering

// Nes_Emu 0.5.6. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_PPU_RENDERING_H
#define NES_PPU_RENDERING_H

#include "nes_state.h"

class Nes_Ppu_Rendering;

template<int clipped>
struct Nes_Ppu_Rendering_
{
	static void draw_bg( Nes_Ppu_Rendering&, int scanline, int skip, int height );
	static void draw_sprite( Nes_Ppu_Rendering&, byte const* sprite, int begin, int end );
};

class Nes_Ppu_Rendering : public ppu_state_t {
protected:
	enum { bytes_per_tile = 16 };
public:
	typedef BOOST::uint8_t byte;
	typedef BOOST::uint32_t uint32_t;
	
	enum { image_width = 256 };
	enum { image_height = 240 };
	enum { image_top = 0 };
	enum { image_left = 8 };
	enum { buffer_width = image_width + 16 };
	enum { buffer_height = image_height };
	
	// Sprite RAM
	byte spr_ram [0x100];
	byte sprite_scanlines [image_height]; // number of sprites on each scanline
	
	// Nametable and CHR RAM
	enum { nt_ram_size = 0x1000 };
	enum { chr_tile_count = 0x200 };
	enum { chr_addr_size = chr_tile_count * bytes_per_tile };
	enum { mini_offscreen_height = 16 }; // height of double-height sprite
	struct impl_t
	{
		byte nt_ram [nt_ram_size];
		BOOST::uint32_t clip_buf [256 * 2];
		byte chr_ram [chr_addr_size];
		byte mini_offscreen [buffer_width * mini_offscreen_height];
	};
	impl_t* impl;
	
protected:
	int sprite_hit_x;
	int sprite_hit_y;
	int sprite_height() const { return ((w2000 >> 2) & 8) + 8; }
	enum { max_sprites = 64 };
	
private:
	friend class Nes_Ppu_Impl;
	friend class Nes_Ppu_Rendering_<0>;
	friend class Nes_Ppu_Rendering_<1>;
	
	// all state is set up by Nes_Ppu_Impl
	
	// graphics output
	BOOST::uint32_t palette_offset;
	byte* scanline_pixels;
	long scanline_row_bytes;
	
	// memory  mapping
	enum { chr_page_size = 0x400 };
	long chr_pages [chr_addr_size / chr_page_size];
	long map_chr_addr( unsigned ) const;
	byte nt_banks_ [4];
	byte* get_nametable( int addr );
	
	// CHR cache
	typedef BOOST::uint32_t cache_t;
	typedef cache_t cached_tile_t [4];
	cached_tile_t* tile_cache;
	cached_tile_t* flipped_tiles;
	cached_tile_t const& get_tile( int index, bool h_flip = false ) const;
	
	// rendering
	void fill_background( int count );
	void clip_left( int count );
	void save_left( int count );
	void check_sprite_hit( int begin, int end );
	void draw_sprites( int begin, int end );
	void restore_left( int count );
};

inline Nes_Ppu_Rendering::cached_tile_t const& Nes_Ppu_Rendering::get_tile( int i, bool h_flip ) const
{
	return (h_flip ? flipped_tiles : tile_cache)
			[(unsigned long) map_chr_addr( i * bytes_per_tile ) / bytes_per_tile];
}

inline byte* Nes_Ppu_Rendering::get_nametable( int addr )
{
	return &impl->nt_ram [nt_banks_ [(addr >> 10) & 3] * 0x400];
}

inline long Nes_Ppu_Rendering::map_chr_addr( unsigned addr ) const
{
	return chr_pages [addr / chr_page_size] + addr % chr_page_size;
}

#endif

