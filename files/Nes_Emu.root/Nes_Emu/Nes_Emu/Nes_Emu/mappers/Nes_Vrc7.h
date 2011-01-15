
// Konami VRC7 sound chip emulator

// Nes_Snd_Emu 0.1.7. Copyright (C) 2003-2005 Shay Green. GNU LGPL license.

#ifndef NES_VRC7_H
#define NES_VRC7_H

struct vrc7_snapshot_t;

class Nes_Vrc7 {
public:
	Nes_Vrc7();
	~Nes_Vrc7();
	
	// See Nes_Apu.h for reference
	void reset();
	void volume( double );
	void treble_eq( blip_eq_t const& );
	void output( Blip_Buffer* );
	enum { osc_count = 6 };
	void osc_output( int index, Blip_Buffer* );
	void end_frame( nes_time_t );
	void save_snapshot( vrc7_snapshot_t* ) const;
	void load_snapshot( vrc7_snapshot_t const& );
	
	void write_reg( int reg );
	void write_data( nes_time_t, int data );
	
private:
	// noncopyable
	Nes_Vrc7( const Nes_Vrc7& );
	Nes_Vrc7& operator = ( const Nes_Vrc7& );

	struct Vrc7_Osc
	{
		BOOST::uint8_t regs [3];
		Blip_Buffer* output;
		int last_amp;
	};

	void * opll;
	nes_time_t last_time;
	
	Blip_Synth<blip_med_quality,2048*2> synth; // DB2LIN_AMP_BITS == 11, * 2
	int count;
	Vrc7_Osc oscs [osc_count];
	
	void run_until( nes_time_t );
};

struct vrc7_snapshot_t
{
	BOOST::uint8_t latch;
	BOOST::uint8_t inst [8];
	BOOST::uint8_t regs [6] [3];
	BOOST::uint8_t count;
};
BOOST_STATIC_ASSERT( sizeof (vrc7_snapshot_t) == 28 );

inline void Nes_Vrc7::osc_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = buf;
}

#endif

