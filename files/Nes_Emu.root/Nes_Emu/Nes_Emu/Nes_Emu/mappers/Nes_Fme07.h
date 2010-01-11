#ifndef NES_FME07_H
#define NES_FME07_H

#include <nes_emu/Blip_Buffer.h>

struct ay_snapshot_t;

class Nes_Fme07 {
public:
	Nes_Fme07();
	~Nes_Fme07();
	
	// See Nes_Apu.h for reference
	void reset();
	void volume( double );
	void treble_eq( blip_eq_t const& );
	void output( Blip_Buffer* );
	enum { osc_count = 3 };
	void osc_output( int index, Blip_Buffer* );
	void end_frame( nes_time_t );
	void save_snapshot( ay_snapshot_t* ) const;
	void load_snapshot( ay_snapshot_t const& );
	
	void write_reg( int data );
	void write_data( nes_time_t, int data );
	
private:
	// noncopyable
	Nes_Fme07( const Nes_Fme07& );
	Nes_Fme07& operator = ( const Nes_Fme07& );

	// running state
	int             register_latch;
	BOOST::uint8_t  Regs[16];
	int             PeriodA,PeriodB,PeriodC,PeriodN,PeriodE;
	int             CountA,CountB,CountC,CountN,CountE;
	unsigned        VolA,VolB,VolC,VolE;
	BOOST::uint8_t  EnvelopeA,EnvelopeB,EnvelopeC;
	BOOST::uint8_t  OutputA,OutputB,OutputC,OutputN;
	BOOST::int8_t   CountEnv;
	BOOST::uint8_t  Hold,Alternate,Attack,Holding;
	BOOST::uint32_t RNG;

	// initialized on startup
	unsigned VolTable[ 32 ];

	// initialized on reset
	nes_time_t last_time;
	
	Blip_Synth<blip_good_quality,255> synth;
	Blip_Buffer * out[ osc_count ];
	int last_amp[ osc_count ];
	
	void run_until( nes_time_t );
};

struct ay_snapshot_t
{
	BOOST::uint8_t  register_latch; // 1
	BOOST::uint8_t  Regs[16]; // 16
	BOOST::uint16_t PeriodA,PeriodB,PeriodC,PeriodN,PeriodE; // 1b align + 2 * 5
	BOOST::uint16_t CountA,CountB,CountC,CountN,CountE; // 2 * 5
	BOOST::uint8_t  VolA,VolB,VolC,VolE; // 4
	BOOST::uint8_t  EnvelopeA,EnvelopeB,EnvelopeC; // 3
	BOOST::uint8_t  OutputA,OutputB,OutputC,OutputN; // 4
	BOOST::int8_t   CountEnv; // 1
	BOOST::uint8_t  Hold,Alternate,Attack,Holding; // 4
	BOOST::uint32_t RNG; // 2b align + 4
};
BOOST_STATIC_ASSERT( sizeof (ay_snapshot_t) == 60 );

inline void Nes_Fme07::osc_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	out[ i ] = buf;
}

#endif
