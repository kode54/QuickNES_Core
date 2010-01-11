
// NES file formats

// Nes_Emu 0.5.4. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_STATE_H
#define NES_STATE_H

#include "blargg_common.h"
#include "apu_snapshot.h"

typedef long nes_tag_t;

typedef BOOST::uint8_t byte;

// Binary format of save state blocks. All multi-byte values are stored in little-endian.

nes_tag_t const snapshot_file_tag = 'NESS';

nes_tag_t const movie_file_tag = 'NMOV';

// Name of ROM in 8-bit characters (ASCII or whatever) with ".nes" etc *removed*,
// no NUL termination. Yes: "Castlevania (U)". No: "Strider (U).nes".
nes_tag_t const rom_name_tag = 'romn';

// name of ROM in 16-bit Unicode, no NUL termination
nes_tag_t const rom_unicode_name_tag = 'romu';

struct nes_block_t
{
	BOOST::uint32_t tag; // ** stored in big-endian
	BOOST::uint32_t size;
	
	void swap();
};
BOOST_STATIC_ASSERT( sizeof (nes_block_t) == 8 );

unsigned long const group_begin_size = 0xffffffff; // group block has this size
nes_tag_t const group_end_tag = 'gend';            // group end block has this tag

struct movie_info_t
{
	BOOST::uint32_t begin;
	BOOST::uint32_t length;
	BOOST::uint16_t period;
	BOOST::uint16_t extra;
	byte joypad_count;
	byte has_joypad_sync;
	byte unused [2];
	
	enum { tag = 'INFO' };
	void swap();
};
BOOST_STATIC_ASSERT( sizeof (movie_info_t) == 16 );

struct nes_state_t
{
	BOOST::uint16_t timestamp; // CPU clocks * 15 (for NTSC)
	byte pal;
	byte unused [1];
	BOOST::uint32_t frame_count; // number of frames emulated since power-up
	
	enum { tag = 'TIME' };
	void swap();
};
BOOST_STATIC_ASSERT( sizeof (nes_state_t) == 8 );

struct joypad_state_t
{
	BOOST::uint32_t joypad_latches [2]; // joypad 1 & 2 shift registers
	byte w4016;             // strobe
	byte unused [3];
	
	enum { tag = 'CTRL' };
	void swap();
};
BOOST_STATIC_ASSERT( sizeof (joypad_state_t) == 12 );

int const max_mapper_state_size = 64;
struct mapper_state_t
{
	byte state [max_mapper_state_size];
};
BOOST_STATIC_ASSERT( sizeof (mapper_state_t) == max_mapper_state_size );

struct cpu_state_t
{
	BOOST::uint16_t pc;
	byte s;
	byte p;
	byte a;
	byte x;
	byte y;
	byte unused [1];
	
	enum { tag = 'CPUR' };
	void swap();
};
BOOST_STATIC_ASSERT( sizeof (cpu_state_t) == 8 );

struct ppu_state_t
{
	byte w2000;                 // control
	byte w2001;                 // control
	byte r2002;                 // status
	byte w2003;                 // sprite ram addr
	byte r2007;                 // vram read buffer
	byte second_write;          // true if next write to $2005/$2006 sets high bits
	BOOST::uint16_t vram_addr;  // loopy_v
	BOOST::uint16_t vram_temp;  // loopy_t
	byte pixel_x;               // fine-scroll (0-7)
	byte unused [1];
	byte palette [0x20];        // entries $10, $14, $18, $1c should be ignored
	
	enum { tag = 'PPUR' };
	void swap();
};
BOOST_STATIC_ASSERT( sizeof (ppu_state_t) == 12 + 0x20 );

struct mmc1_state_t
{
	byte regs [4]; // current registers (5 bits each)
	byte bit;      // number of bits in buffer (0 to 4)
	byte buf;      // currently buffered bits (new bits added to bottom)
};
BOOST_STATIC_ASSERT( sizeof (mmc1_state_t) == 6 );

struct mmc3_state_t
{
	byte banks [8]; // last writes to $8001 indexed by (mode & 7)
	byte mode;      // $8000
	byte mirror;    // $a000
	byte sram_mode; // $a001
	byte irq_ctr;   // internal counter
	byte irq_latch; // $c000
	byte irq_enabled;// last write was to 0) $e000, 1) $e001
	byte irq_flag;
};
BOOST_STATIC_ASSERT( sizeof (mmc3_state_t) == 15 );

#endif

