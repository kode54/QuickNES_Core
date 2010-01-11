#include "stdafx.h"

#include <nes_emu/blargg_source.h>

#if 0 // what the hell is this?
struct m255_state_t
{
	byte addr [2];
	byte junk [4];
};

BOOST_STATIC_ASSERT( sizeof (m255_state_t) == 6 );

class Mapper_255 : public Nes_Mapper, m255_state_t {
public:
	Mapper_255()
	{
		m255_state_t * state = this;
		register_state( state, sizeof * state );
	}
	
	void reset_state()
	{
		memcpy( junk, "\17\17\0\0", sizeof junk );
		write( 0, 0, 0 );
	}

	void apply_mapping()
	{
		intercept_reads( 0x5800, 0x6000 - 0x5800 );
		intercept_writes( 0x5800, 0x6000 - 0x5800 );
		write( 0, addr [1] * 256 + addr [0] , 0 );
	}
	
	bool write_intercepted( nes_time_t, nes_addr_t addr, int data )
	{
		if ( ( addr - 0x5800 ) >= ( 0x6000 - 0x5800 ) )
			return false;
		
		junk [addr & 3] = data & 0x0F;
		
		return true;
	}
	
	void write( nes_time_t, nes_addr_t addr, int )
	{
		this->addr [0] = addr & 255;
		this->addr [1] = ( addr >> 8 ) & 127;

		unsigned bank_base    = ( addr >> 14 ) & 1;
		unsigned bank_program = ( addr >>  7 ) & 0x1F;
		unsigned bank_chr     =   addr         & 0x3F;
		
		if( addr & 0x1000 )
		{
			unsigned bank = ( ( bank_program | ( bank_base << 5 ) ) << 1 ) | ( ( addr & 0x40 ) >> 6 );
			set_prg_bank( 0x8000, bank_16k, bank );
			set_prg_bank( 0xC000, bank_16k, bank );
		}
		else
		{
			set_prg_bank( 0x8000, bank_32k, bank_program | ( bank_base << 5 ) );
		}
		if ( addr & ( 1 << 13 ) )
			mirror_horiz();
		else
			mirror_vert();
		set_chr_bank( 0, bank_8k, ( bank_base << 6 ) | bank_chr );
	}

	int read( nes_time_t time, nes_addr_t addr )
	{
		if ( ( addr - 0x5800 ) < ( 0x6000 - 0x5800 ) )
		{
			return junk [addr & 3];
		}

		return Nes_Mapper::read( time, addr );
	}
};
#endif

struct vrc2_state_t
{
	byte prg_banks [2];
	byte chr_banks [8];
	byte mirroring;
	byte security;
};
BOOST_STATIC_ASSERT( sizeof (vrc2_state_t) == 12 );

template <bool type_a>
class Mapper_Vrc2 : public Nes_Mapper, vrc2_state_t {
	int chrShift, prgLineA, prgLineB;

public:
	Mapper_Vrc2()
	{
		chrShift = type_a ? 1 : 0;
		prgLineA = type_a ? 0 : 1;
		prgLineB = type_a ? 1 : 0;

		vrc2_state_t * state = this;
		register_state( state, sizeof * state );
	}

	void reset_state()
	{
		security = 0;
	}

	void apply_mapping()
	{
		intercept_reads( 0x6000, bank_8k );
		intercept_writes( 0x6000, bank_8k );

		set_prg_bank( 0x8000, bank_8k, prg_banks [0] );
		set_prg_bank( 0xA000, bank_8k, prg_banks [1] );
		for ( int i = 0; i < (int) sizeof chr_banks; i++ )
			set_chr_bank( i * 0x400, bank_1k, chr_banks [i] );
		set_mirroring();
	}

	bool write_intercepted( nes_time_t, nes_addr_t addr, int data )
	{
		if ( ( addr - 0x6000 ) >= ( 0x8000 - 0x6000 ) )
			return false;

		security = data & 1;

		return true;
	}

	void write( nes_time_t, nes_addr_t addr, int data )
	{
		switch ( addr & 0xF000 )
		{
		case 0x8000:
			prg_banks [0] = data;
			set_prg_bank( 0x8000, bank_8k, data );
			return;

		case 0x9000:
			mirroring = data;
			set_mirroring();
			return;

		case 0xA000:
			prg_banks [1] = data;
			set_prg_bank( 0xA000, bank_8k, data );
			return;
		}

		switch ( ( addr & 0xF000 ) | ( addr << ( 9 - prgLineA ) & 0x200 ) | ( addr << ( 8 - prgLineB ) & 0x100 ) )
		{
		case 0xB000: set_chr<0>( 0, data ); break;
		case 0xB100: set_chr<4>( 0, data ); break;
		case 0xB200: set_chr<0>( 1, data ); break;
		case 0xB300: set_chr<4>( 1, data ); break;
		case 0xC000: set_chr<0>( 2, data ); break;
		case 0xC100: set_chr<4>( 2, data ); break;
		case 0xC200: set_chr<0>( 3, data ); break;
		case 0xC300: set_chr<4>( 3, data ); break;
		case 0xD000: set_chr<0>( 4, data ); break;
		case 0xD100: set_chr<4>( 4, data ); break;
		case 0xD200: set_chr<0>( 5, data ); break;
		case 0xD300: set_chr<4>( 5, data ); break;
		case 0xE000: set_chr<0>( 6, data ); break;
		case 0xE100: set_chr<4>( 6, data ); break;
		case 0xE200: set_chr<0>( 7, data ); break;
		case 0xE300: set_chr<4>( 7, data ); break;
		}
	}

	int read( nes_time_t time, nes_addr_t addr )
	{
		if ( ( addr - 0x6000 ) < ( 0x8000 - 0x6000 ) )
		{
			return security;
		}

		return Nes_Mapper::read( time, addr );
	}

private:
	void set_mirroring()
	{
		switch ( mirroring & 3 )
		{
		case 0: mirror_vert(); break;
		case 1: mirror_horiz(); break;
		case 2:
		case 3: mirror_single( mirroring & 1 ); break;
		}
	}

	template <unsigned offset>
	void set_chr( unsigned banknumber, unsigned subbank )
	{
		unsigned bank = ( chr_banks [banknumber] & 0xF0 >> offset ) | ( ( subbank >> chrShift & 0x0F ) << offset );
		chr_banks [banknumber] = bank;
		set_chr_bank( banknumber * 0x400, bank_1k, bank );
	}
};

struct m156_state_t
{
	byte prg_bank;
	byte chr_banks [8];
};
BOOST_STATIC_ASSERT( sizeof (m156_state_t) == 9 );

class Mapper_156 : public Nes_Mapper, m156_state_t {
public:
	Mapper_156()
	{
		m156_state_t * state = this;
		register_state( state, sizeof * state );
	}
	
	void reset_state()
	{
		prg_bank = 0;
		for ( unsigned i = 0; i < 8; i++ ) chr_banks [i] = i;
		enable_sram();
		apply_mapping();
	}

	void apply_mapping()
	{
		mirror_single( 0 );
		set_prg_bank( 0x8000, bank_16k, prg_bank );
		
		for ( int i = 0; i < (int) sizeof chr_banks; i++ )
			set_chr_bank( i * 0x400, bank_1k, chr_banks [i] );
	}
	
	void write( nes_time_t, nes_addr_t addr, int data )
	{
		unsigned int reg = addr - 0xC000;
		if ( addr == 0xC010 )
		{
			prg_bank = data;
			set_prg_bank( 0x8000, bank_16k, data );
		}
		else if ( reg < 4 )
		{
			chr_banks [reg] = data;
			set_chr_bank( reg * 0x400, bank_1k, data );
		}
		else if ( ( reg - 8 ) < 4 )
		{
			reg -= 4;
			chr_banks [reg] = data;
			set_chr_bank( reg * 0x400, bank_1k, data );
		}
	}
};

void register_more_mappers();
void register_more_mappers()
{
	register_mapper< Mapper_Vrc2<true> > ( 22 );
	register_mapper< Mapper_Vrc2<false> > ( 23 );
	register_mapper< Mapper_156 > ( 156 );
}
