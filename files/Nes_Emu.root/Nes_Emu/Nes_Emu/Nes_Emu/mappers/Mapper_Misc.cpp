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
		if ( ( addr - 0x5800 ) > ( 0x6000 - 0x5800 ) )
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
		for ( unsigned i = 0; i < 7; i++ ) chr_banks [i] = i;
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
	register_mapper< Mapper_156 > ( 156 );
}
