
// Nes_Emu 0.5.4. http://www.slack.net/~ant/nes-emu/

#include "Nes_Cpu.h"

#include <string.h>
#include <limits.h>

#include "blargg_endian.h"

/* Copyright (C) 2003-2005 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details. You should have received a copy of the GNU Lesser General
Public License along with this module; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include BLARGG_SOURCE_BEGIN

Nes_Cpu::Nes_Cpu()
{
	callback_data = NULL;
	reset();
}

inline void Nes_Cpu::set_code_page( int i, uint8_t const* p )
{
	#if BLARGG_NONPORTABLE
		p -= i * page_size;
	#endif
	code_map [i] = p;
}

void Nes_Cpu::reset( const void* unmapped_code_page, reader_t read, writer_t write )
{
	r.status = 0;
	r.sp = 0;
	r.pc = 0;
	r.a = 0;
	r.x = 0;
	r.y = 0;
	
	clock_count = 0;
	base_time = 0;
	clock_limit = 0;
	
	for ( int i = 0; i < page_count + 1; i++ )
	{
		set_code_page( i, (uint8_t*) unmapped_code_page );
		data_reader [i] = read;
		data_writer [i] = write;
	}
}

void Nes_Cpu::map_code( nes_addr_t start, unsigned long size, const void* data )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size % page_size == 0 );
	
	unsigned first_page = start / page_size;
	for ( unsigned i = size / page_size; i--; )
		set_code_page( first_page + i, (uint8_t*) data + i * page_size );
}

void Nes_Cpu::set_reader( nes_addr_t start, unsigned long size, reader_t func )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size % page_size == 0 );
	unsigned first_page = start / page_size;
	for ( unsigned i = size / page_size; i--; )
		data_reader [first_page + i] = func;
}

void Nes_Cpu::set_writer( nes_addr_t start, unsigned long size, writer_t func )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size % page_size == 0 );
	unsigned first_page = start / page_size;
	for ( unsigned i = size / page_size; i--; )
		data_writer [first_page + i] = func;
}

// Note: 'addr' is evaulated more than once in the following macros, so it
// must not contain side-effects.

#define READ( addr )        (data_reader [(addr) >> page_bits]( callback_data, addr ))
#define WRITE( addr, data ) (data_writer [(addr) >> page_bits]( callback_data, addr, data ))

#define READ_LOW( addr )        (low_mem [int (addr)])
#define WRITE_LOW( addr, data ) void (READ_LOW( addr ) = (data))

#if BLARGG_NONPORTABLE
	#define READ_PROG( addr )   (code_map [(addr) >> page_bits] [addr])
#else
	#define READ_PROG( addr )   (code_map [(addr) >> page_bits] [(addr) & (page_size - 1)])
#endif
#define READ_PROG16( addr ) GET_LE16( &READ_PROG( addr ) )

typedef int sp_t;

#define SET_SP( v )     (sp = (v))
#define GET_SP()        (sp + 0)

#define PUSH( v )       (WRITE_LOW( 0x100 + sp, v ), void (sp = (sp - 1) & 0xff))
#define POP()           (sp = (sp + 1) & 0xff, READ_LOW( 0x100 + sp ))

int Nes_Cpu::read( nes_addr_t addr )
{
	return READ( addr );
}

void Nes_Cpu::write( nes_addr_t addr, int value )
{
	WRITE( addr, value );
}

#ifndef NES_CPU_GLUE_ONLY

static const unsigned char clock_table [256] = {                             
	7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6,
	2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
	6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6,
	2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
	6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6,
	2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
	6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6,
	2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
	2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,
	2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,
	2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,
	2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,
	2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,
	2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
	2,6,3,8,3,3,5,5,2,2,2,2,4,4,6,6,
	2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7
};

#include BLARGG_ENABLE_OPTIMIZER

Nes_Cpu::result_t Nes_Cpu::run( nes_time_t end )
{
	end_time( end );
	
	volatile result_t result = result_cycles;
	
#if BLARGG_CPU_POWERPC
	// cache commonly-used values in registers
	long clock_count = this->clock_count;
	writer_t* const data_writer = this->data_writer;
	reader_t* const data_reader = this->data_reader;
	uint8_t* const low_mem = this->low_mem;
#endif
	
	// registers
	unsigned pc = r.pc;
	sp_t sp;
	SET_SP( r.sp );
	int a = r.a;
	int x = r.x;
	int y = r.y;
	
	// status flags
	
	#define IS_NEG (nz & 0x880)
	
	const int st_n = 0x80;
	const int st_v = 0x40;
	const int st_r = 0x20;
	const int st_b = 0x10;
	const int st_d = 0x08;
	const int st_i = 0x04;
	const int st_z = 0x02;
	const int st_c = 0x01;
	
	#define CALC_STATUS( out ) do {             \
		out = status & (st_v | st_d | st_i);    \
		out |= (c >> 8) & st_c;                 \
		if ( IS_NEG ) out |= st_n;              \
		if ( !(nz & 0xFF)) out |= st_z;         \
	} while ( false )

	#define SET_STATUS( in ) do {               \
		status = in & (st_v | st_d | st_i);     \
		c = in << 8;                            \
		nz = (in << 4) & 0x800;                 \
		nz |= ~in & st_z;                       \
	} while ( false )
	
	uint8_t status;
	int c;  // store C as 'c' & 0x100.
	int nz; // store Z as 'nz' & 0xFF == 0. see above for encoding of N.
	{
		int temp = r.status;
		SET_STATUS( temp );
	}

	goto loop;
	
	unsigned data;
	
branch_taken:
	{
		clock_count++;
		int offset = (BOOST::int8_t) data;
		uint8_t page = pc + 1;
		pc += offset;
		clock_count += ((page + offset) >> 8) & 1;
	}
inc_pc_loop:            
	pc++;
loop:
	
	// to do: handle pc wrap-around? can be mostly handled by trapping
	// access to unmapped page at 0x10000; the only case this doesn't
	// handle is when a multi-byte instruction crosses the boundary.
	check( unsigned (pc) < 0x10000 );
	
	check( unsigned (GET_SP()) < 0x100 );
	assert( unsigned (a) < 0x100 );
	assert( unsigned (x) < 0x100 );
	assert( unsigned (y) < 0x100 );
	
	uint8_t const* code = &READ_PROG( pc );
	pc++;
	unsigned opcode = code [0];
	
	if ( clock_count >= clock_limit )
		goto stop;
	
	data = code [1]; // to do: if pc is at end of page, this might get wrong byte
	#ifndef NDEBUG
		if ( opcode != 0x60 && &READ_PROG( pc ) != &code [1] )
		{
			dprintf( "READ_PROG( %04X ) = %p, code [1] = %p, opcode = %02X\n",
					(int) pc, &READ_PROG( pc ), &code [1], (int) opcode ); 
			data = READ_PROG( pc );
		}
	#endif
	
	clock_count += clock_table [opcode];
	#if BLARGG_CPU_POWERPC
		this->clock_count = clock_count;
	#endif
	
	switch ( opcode )
	{

// Macros

#define ADD_PAGE        (pc++, data += 0x100 * READ_PROG( pc ));
#define GET_ADDR()      READ_PROG16( pc )

#define HANDLE_PAGE_CROSSING( lsb ) clock_count += (lsb) >> 8;

#define INC_DEC_XY( reg, n )        \
	reg = uint8_t (nz = reg + n);   \
	goto loop;

#define IND_Y {                                                 \
		int temp = READ_LOW( data ) + y;                        \
		data = temp + 0x100 * READ_LOW( uint8_t (data + 1) );   \
		HANDLE_PAGE_CROSSING( temp );                           \
	}
	
#define IND_X {                                                 \
		int temp = data + x;                                    \
		data = 0x100 * READ_LOW( uint8_t (temp + 1) ) + READ_LOW( uint8_t (temp) ); \
	}
	
#define ARITH_ADDR_MODES( op )          \
case op - 0x04: /* (ind,x) */           \
	IND_X                               \
	goto ptr##op;                       \
case op + 0x0C: /* (ind),y */           \
	IND_Y                               \
	goto ptr##op;                       \
case op + 0x10: /* zp,X */              \
	data = uint8_t (data + x);          \
case op + 0x00: /* zp */                \
	data = READ_LOW( data );            \
	goto imm##op;                       \
case op + 0x14: /* abs,Y */             \
	data += y;                          \
	goto ind##op;                       \
case op + 0x18: /* abs,X */             \
	data += x;                          \
ind##op:                                \
	HANDLE_PAGE_CROSSING( data );       \
case op + 0x08: /* abs */               \
	ADD_PAGE                            \
ptr##op:                                \
	data = READ( data );                \
case op + 0x04: /* imm */               \
imm##op:                                \

#define BRANCH( cond )      \
	if ( cond )             \
		goto branch_taken;  \
	goto inc_pc_loop;

// Often-Used

	case 0xB5: // LDA zp,x
		data = uint8_t (data + x);
	case 0xA5: // LDA zp
		a = nz = READ_LOW( data );
		goto inc_pc_loop;
	
	case 0xD0: // BNE
		BRANCH( (uint8_t) nz );
	
	case 0x4C: // JMP abs
		pc++;
		goto jmp_common;
	
	case 0x20: // JSR
		pc++;
		PUSH( pc >> 8 );
		PUSH( pc );
	jmp_common:
		pc = data + 0x100 * READ_PROG( pc );
		goto loop;
	
	case 0xE8: INC_DEC_XY( x, 1 )  // INX
	
	case 0x10: // BPL
		BRANCH( !IS_NEG )
	
	ARITH_ADDR_MODES( 0xC5 ) // CMP
		nz = a - data;
		c = ~nz;
		nz &= 0xff;
		goto inc_pc_loop;
	
	case 0x30: // BMI
		BRANCH( IS_NEG )
	
	case 0xF0: // BEQ
		BRANCH( !(uint8_t) nz );
	
	case 0x95: // STA zp,x
		data = uint8_t (data + x);
	case 0x85: // STA zp
		WRITE_LOW( data, a );
		goto inc_pc_loop;
	
	case 0xC8: INC_DEC_XY( y, 1 )  // INY

	case 0xA8: // TAY
		y = a;
	case 0x98: // TYA
		a = nz = y;
		goto loop;
	
	case 0xBD: // LDA abs,X
		data += x;
	lda_ind_common:
		HANDLE_PAGE_CROSSING( data );
	case 0xAD: // LDA abs
		ADD_PAGE
	lda_ptr:
		nz = a = READ( data );
		goto inc_pc_loop;

	case 0x60: // RTS
		pc = POP();
		pc += 0x100 * POP();
		goto inc_pc_loop;

	case 0x9D: // STA abs,X
		data += x;
	sta_ind_common:
		HANDLE_PAGE_CROSSING( data );
	case 0x8D: // STA abs
		ADD_PAGE
	sta_ptr:
		WRITE( data, a );
		goto inc_pc_loop;
	
	case 0xA9: // LDA #imm
		a = nz = data;
		goto inc_pc_loop;

// Branch

	case 0x50: // BVC
		BRANCH( !(status & st_v) )
	
	case 0x70: // BVS
		BRANCH( status & st_v )
	
	case 0xB0: // BCS
		BRANCH( c & 0x100 )
	
	case 0x90: // BCC
		BRANCH( !(c & 0x100) )
	
// Load/store
	
	case 0x94: // STY zp,x
		data = uint8_t (data + x);
	case 0x84: // STY zp
		WRITE_LOW( data, y );
		goto inc_pc_loop;
	
	case 0x96: // STX zp,y
		data = uint8_t (data + y);
	case 0x86: // STX zp
		WRITE_LOW( data, x );
		goto inc_pc_loop;
	
	case 0xB6: // LDX zp,y
		data = uint8_t (data + y);
	case 0xA6: // LDX zp
		data = READ_LOW( data );
	case 0xA2: // LDX imm
		x = data;
		nz = data;
		goto inc_pc_loop;
	
	case 0xB4: // LDY zp,x
		data = uint8_t (data + x);
	case 0xA4: // LDY zp
		data = READ_LOW( data );
	case 0xA0: // LDY imm
		y = data;
		nz = data;
		goto inc_pc_loop;
	
	case 0xB1: // LDA (ind),Y
		IND_Y
		goto lda_ptr;
	
	case 0xA1: // LDA (ind,X)
		IND_X
		goto lda_ptr;
	
	case 0xB9: // LDA abs,Y
		data += y;
		goto lda_ind_common;
	
	case 0x91: // STA (ind),Y
		IND_Y
		goto sta_ptr;
	
	case 0x81: // STA (ind,X)
		IND_X
		goto sta_ptr;
	
	case 0x99: // STA abs,Y
		data += y;
		goto sta_ind_common;
	
	case 0xBC: // LDY abs,X
		data += x;
		HANDLE_PAGE_CROSSING( data );
	case 0xAC:{// LDY abs
		pc++;
		unsigned addr = data + 0x100 * READ_PROG( pc );
		y = nz = READ( addr );
		goto inc_pc_loop;
	}
	
	case 0xBE: // LDX abs,y
		data += y;
		HANDLE_PAGE_CROSSING( data );
	case 0xAE:{// LDX abs
		pc++;
		unsigned addr = data + 0x100 * READ_PROG( pc );
		x = nz = READ( addr );
		goto inc_pc_loop;
	}
	
	{
		int temp;
	case 0x8C: // STY abs
		temp = y;
		goto store_abs;
	
	case 0x8E: // STX abs
		temp = x;
	store_abs:
		unsigned addr = GET_ADDR();
		WRITE( addr, temp );
		pc += 2;
		goto loop;
	}

// Compare

	case 0xEC:{// CPX abs
		unsigned addr = GET_ADDR();
		pc++;
		data = READ( addr );
		goto cpx_data;
	}
	
	case 0xE4: // CPX zp
		data = READ_LOW( data );
	case 0xE0: // CPX imm
	cpx_data:
		nz = x - data;
		c = ~nz;
		nz &= 0xff;
		goto inc_pc_loop;
	
	case 0xCC:{// CPY abs
		unsigned addr = GET_ADDR();
		pc++;
		data = READ( addr );
		goto cpy_data;
	}
	
	case 0xC4: // CPY zp
		data = READ_LOW( data );
	case 0xC0: // CPY imm
	cpy_data:
		nz = y - data;
		c = ~nz;
		nz &= 0xff;
		goto inc_pc_loop;
	
// Logical

	ARITH_ADDR_MODES( 0x25 ) // AND
		nz = (a &= data);
		goto inc_pc_loop;
	
	ARITH_ADDR_MODES( 0x45 ) // EOR
		nz = (a ^= data);
		goto inc_pc_loop;
	
	ARITH_ADDR_MODES( 0x05 ) // ORA
		nz = (a |= data);
		goto inc_pc_loop;
	
	case 0x2C:{// BIT abs
		unsigned addr = GET_ADDR();
		pc++;
		nz = READ( addr );
		goto bit_common;
	}
	
	case 0x24: // BIT zp
		nz = READ_LOW( data );
	bit_common:
		status &= ~st_v;
		status |= nz & st_v;
		// if result is zero, might also be negative, so use secondary N bit
		if ( !(a & nz) )
			nz = (nz << 4) & 0x800;
		goto inc_pc_loop;
		
// Add/subtract

	ARITH_ADDR_MODES( 0xE5 ) // SBC
	case 0xEB: // unofficial equivalent
		data ^= 0xFF;
		goto adc_imm;
	
	ARITH_ADDR_MODES( 0x65 ) // ADC
	adc_imm: {
		int carry = (c >> 8) & 1;
		int ov = (a ^ 0x80) + carry + (BOOST::int8_t) data; // sign-extend
		status &= ~st_v;
		status |= (ov >> 2) & 0x40;
		c = nz = a + data + carry;
		a = (uint8_t) nz;
		goto inc_pc_loop;
	}
	
// Shift/rotate

	case 0x4A: // LSR A
		c = 0;
	case 0x6A: // ROR A
		nz = (c >> 1) & 0x80; // could use bit insert macro here
		c = a << 8;
		nz |= a >> 1;
		a = nz;
		goto loop;

	case 0x0A: // ASL A
		nz = a << 1;
		c = nz;
		a = (uint8_t) nz;
		goto loop;

	case 0x2A: { // ROL A
		nz = a << 1;
		int temp = (c >> 8) & 1;
		c = nz;
		nz |= temp;
		a = (uint8_t) nz;
		goto loop;
	}
	
	case 0x3E: // ROL abs,X
		data += x;
		goto rol_abs;
	
	case 0x1E: // ASL abs,X
		data += x;
	case 0x0E: // ASL abs
		c = 0;
	case 0x2E: // ROL abs
	rol_abs:
		HANDLE_PAGE_CROSSING( data );
		ADD_PAGE
		nz = (c >> 8) & 1;
		nz |= (c = READ( data ) << 1);
	rotate_common:
		WRITE( data, (uint8_t) nz );
		goto inc_pc_loop;
	
	case 0x7E: // ROR abs,X
		data += x;
		goto ror_abs;
	
	case 0x5E: // LSR abs,X
		data += x;
	case 0x4E: // LSR abs
		c = 0;
	case 0x6E: // ROR abs
	ror_abs: {
		HANDLE_PAGE_CROSSING( data );
		ADD_PAGE
		int temp = READ( data );
		nz = ((c >> 1) & 0x80) | (temp >> 1);
		c = temp << 8;
		goto rotate_common;
	}
	
	case 0x76: // ROR zp,x
		data = uint8_t (data + x);
		goto ror_zp;
	
	case 0x56: // LSR zp,x
		data = uint8_t (data + x);
	case 0x46: // LSR zp
		c = 0;
	case 0x66: // ROR zp
	ror_zp: {
		int temp = READ_LOW( data );
		nz = ((c >> 1) & 0x80) | (temp >> 1);
		c = temp << 8;
		goto write_nz_zp;
	}
	
	case 0x36: // ROL zp,x
		data = uint8_t (data + x);
		goto rol_zp;
	
	case 0x16: // ASL zp,x
		data = uint8_t (data + x);
	case 0x06: // ASL zp
		c = 0;
	case 0x26: // ROL zp
	rol_zp:
		nz = (c >> 8) & 1;
		nz |= (c = READ_LOW( data ) << 1);
		goto write_nz_zp;
	
// Increment/decrement

	case 0xCA: INC_DEC_XY( x, -1 ) // DEX
	
	case 0x88: INC_DEC_XY( y, -1 ) // DEY
	
	case 0xF6: // INC zp,x
		data = uint8_t (data + x);
	case 0xE6: // INC zp
		nz = 1;
		goto add_nz_zp;
	
	case 0xD6: // DEC zp,x
		data = uint8_t (data + x);
	case 0xC6: // DEC zp
		nz = -1;
	add_nz_zp:
		nz += READ_LOW( data );
	write_nz_zp:
		WRITE_LOW( data, nz );
		goto inc_pc_loop;
	
	case 0xFE: // INC abs,x
		HANDLE_PAGE_CROSSING( data + x );
		data = x + GET_ADDR();
		goto inc_ptr;
	
	case 0xEE: // INC abs
		data = GET_ADDR();
	inc_ptr:
		nz = 1;
		goto inc_common;
	
	case 0xDE: // DEC abs,x
		HANDLE_PAGE_CROSSING( data + x );
		data = x + GET_ADDR();
		goto dec_ptr;
	
	case 0xCE: // DEC abs
		data = GET_ADDR();
	dec_ptr:
		nz = -1;
	inc_common:
		nz += READ( data );
		pc += 2;
		WRITE( data, (uint8_t) nz );
		goto loop;
		
// Transfer

	case 0xAA: // TAX
		x = a;
	case 0x8A: // TXA
		a = nz = x;
		goto loop;

	case 0x9A: // TXS
		SET_SP( x ); // verified (no flag change)
		goto loop;
	
	case 0xBA: // TSX
		x = nz = GET_SP();
		goto loop;
	
// Stack
	
	case 0x48: // PHA
		PUSH( a ); // verified
		goto loop;
		
	case 0x68: // PLA
		a = nz = POP();
		goto loop;
		
	{
		int temp;
	case 0x40: // RTI
		temp = POP();
		pc = POP();
		pc |= POP() << 8;
		goto set_status;
	
	case 0x28: // PLP
		temp = POP();
	set_status:
		data = status & st_i;
		SET_STATUS( temp );
		if ( !(data & ~status) )
			goto loop;
		result = result_cli; // I flag was just cleared
		goto end;
	}
	
	case 0x08: { // PHP
		int temp;
		CALC_STATUS( temp );
		PUSH( temp | st_b | st_r );
		goto loop;
	}
	
	case 0x6C: // JMP (ind)
		data = GET_ADDR();
		pc = READ( data );
		data++;
		pc |= READ( data ) << 8;
		goto loop;
	
	case 0x00: { // BRK
		pc++;
		PUSH( pc >> 8 );
		PUSH( pc );
		int temp;
		CALC_STATUS( temp );
		PUSH( temp | st_b | st_r );
		status |= st_i;
		pc = READ_PROG16( 0xFFFE );
		goto loop;
	}
	
// Flags

	case 0x38: // SEC
		c = ~0;
		goto loop;
	
	case 0x18: // CLC
		c = 0;
		goto loop;
		
	case 0xB8: // CLV
		status &= ~st_v;
		goto loop;
	
	case 0xD8: // CLD
		status &= ~st_d;
		goto loop;
	
	case 0xF8: // SED
		status |= st_d;
		goto loop;
	
	case 0x58: // CLI
		if ( !(status & st_i) )
			goto loop;
		status &= ~st_i;
		result = result_cli;
		goto end;
		
	case 0x78: // SEI
		status |= st_i;
		goto loop;

// Undocumented

	case 0x0C: case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: // SKW
		pc++;
	case 0x74: case 0x04: case 0x14: case 0x34: case 0x44: case 0x54: case 0x64: // SKB
	case 0x80: case 0x82: case 0x89: case 0xC2: case 0xD4: case 0xE2: case 0xF4:
		goto inc_pc_loop;

	case 0xEA: case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: // NOP
		goto loop;

// Unimplemented
	
	case 0x9B: // TAS
	case 0x9C: // SAY
	case 0x9E: // XAS
	case 0x93: // AXA
	case 0x9F: // AXA
	case 0x0B: // ANC
	case 0x2B: // ANC
	case 0xBB: // LAS
	case 0x4B: // ALR
	case 0x6B: // AAR
	case 0x8B: // XAA
	case 0xAB: // OAL
	case 0xCB: // SAX
	case 0x02: case 0x12: case 0x22: case 0x32: // ?
	case 0x42: case 0x52: case 0x62: case 0x72:
	case 0x92: case 0xB2: case 0xD2: case 0xF2:
	case 0x83: case 0x87: case 0x8F: case 0x97: // AXS
	case 0xA3: case 0xA7: case 0xAF: case 0xB3: case 0xB7: case 0xBF: // LAX
	case 0xE3: case 0xE7: case 0xEF: case 0xF3: case 0xF7: case 0xFB: case 0xFF: // INS
	case 0xC3: case 0xC7: case 0xCF: case 0xD3: case 0xD7: case 0xDB: case 0xDF: // DCM
	case 0x63: case 0x67: case 0x6F: case 0x73: case 0x77: case 0x7B: case 0x7F: // RRA
	case 0x43: case 0x47: case 0x4F: case 0x53: case 0x57: case 0x5B: case 0x5F: // LSE
	case 0x23: case 0x27: case 0x2F: case 0x33: case 0x37: case 0x3B: case 0x3F: // RLA
	case 0x03: case 0x07: case 0x0F: case 0x13: case 0x17: case 0x1B: case 0x1F: // ASO
		result = result_badop;
		goto stop;
	}
	
	// If this fails then the case above is missing an opcode
	assert( false );
	
stop:
	pc--;
end:
	
	{
		int temp;
		CALC_STATUS( temp );
		r.status = temp;
	}
	
	base_time += clock_count;
	clock_limit -= clock_count;
	this->clock_count = 0;
	r.pc = pc;
	r.sp = GET_SP();
	r.a = a;
	r.x = x;
	r.y = y;
	
	return result;
}

#endif

