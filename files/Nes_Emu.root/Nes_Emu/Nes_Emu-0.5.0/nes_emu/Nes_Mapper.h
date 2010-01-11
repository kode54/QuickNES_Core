
// NES mapper interface

// Nes_Emu 0.5.0. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_MAPPER
#define NES_MAPPER

#include "nes_state.h"
#include "Nes_Cpu.h"
#include "abstract_file.h"
class Nes_Emu;

class Nes_Mapper {
public:
	virtual ~Nes_Mapper();
	
	// to do: clean this up
	
	int initial_mirroring;
	bool chr_is_rom;
	class Nes_Vrc6* nes_vrc6_;
	
	void init( Nes_Emu* emu, const byte*, long size );
	
	void default_reset();
	
	const char* name() { return name_; }
	virtual void reset() = 0;
	virtual void write( nes_time_t, nes_addr_t, int data ) = 0;
	enum { no_irq = LONG_MAX / 2 };
	virtual nes_time_t next_irq( nes_time_t present );
	virtual void run_until( nes_time_t );
	virtual void end_frame( nes_time_t );
	
	virtual int save_state( mapper_state_t* );
	virtual void load_state( mapper_state_t const& );

protected:
	Nes_Mapper( const char* name );
	Nes_Mapper( const char* name, void* state, int state_size );
	
	// Apply current mapping state to hardware
	virtual void apply_mapping() = 0;
	
	Nes_Emu* emu() const { return emu_; }
	
	// Set PPU mirroring
	void mirror_single( int page );
	void mirror_horiz( int page = 0 );
	void mirror_vert( int page = 0 );
	void mirror_full();
	
	// prevents accidental mixup of arguments and allows optimization of bank functions
	enum bank_size_t {
		bank_1k = 10,
		bank_2k = 11,
		bank_4k = 12,
		bank_8k = 13,
		bank_16k = 14,
		bank_32k = 15
	};
	
	// Map CHR bank at addr (0-0x1fff) size to bank in CHR
	void set_chr_bank( int vram_addr, bank_size_t, int chr_page );
	
	// Map PRG bank at logical bank to bank in PRG. Use last_bank to select
	// last bank (or before, by subtracting, i.e. last_bank - 1 is next-to-last bank)
	enum { last_bank = -1 };
	void set_prg_bank( int cpu_page, bank_size_t, int prg_page );

	byte const* get_prg() const;
	long get_prg_size() const;
	
private:
	Nes_Emu* emu_;
	const char* name_;
	void* state;
	int state_size;
	byte const* prg;
	long prg_size;
	
	void init( const char* );
	friend Nes_Mapper* make_mapper( int code, Nes_Emu* );
	static Nes_Mapper* make_mmc1();
	static Nes_Mapper* make_mmc3();
	static Nes_Mapper* make_mmc5();
	static Nes_Mapper* make_vrc6a();
	static Nes_Mapper* make_vrc6b();
	static Nes_Mapper* make_fme07();
};

Nes_Mapper* make_mapper( int code, Nes_Emu* );

inline void Nes_Mapper::init( Nes_Emu* emu, const byte* p, long s )
{
	emu_ = emu;
	prg = p;
	prg_size = s;
}

#endif

