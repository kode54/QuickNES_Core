/***************************************************************************

  ay8910.c


  Emulation of the AY-3-8910 / YM2149 sound chip.

  Based on various code snippets by Ville Hallik, Michael Cuddy,
  Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.

***************************************************************************/

#include "stdafx.h"

#include "Nes_Fme07.h"

#define MAX_OUTPUT 255

#define STEP 1
#define STEP_SCALE STEP * 16

/* register id's */
#define AY_AFINE	(0)
#define AY_ACOARSE	(1)
#define AY_BFINE	(2)
#define AY_BCOARSE	(3)
#define AY_CFINE	(4)
#define AY_CCOARSE	(5)
#define AY_NOISEPER	(6)
#define AY_ENABLE	(7)
#define AY_AVOL		(8)
#define AY_BVOL		(9)
#define AY_CVOL		(10)
#define AY_EFINE	(11)
#define AY_ECOARSE	(12)
#define AY_ESHAPE	(13)

Nes_Fme07::Nes_Fme07()
{
	{
		int i;
		double out;


		/* calculate the volume->voltage conversion table */
		/* The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per step) */
		/* The YM2149 still has 16 levels for the tone generators, but 32 for */
		/* the envelope generator (1.5dB per step). */
		out = MAX_OUTPUT;
		for ( i = 31; i > 0 ; --i )
		{
			VolTable[ i ] = out + 0.5;	/* round to nearest */

			out /= 1.188502227;	/* = 10 ^ (1.5/20) = 1.5dB */
		}
		VolTable[ 0 ] = 0;
	}

	output( NULL );
	volume( 1.0 );
	reset();
}

Nes_Fme07::~Nes_Fme07()
{
}

void Nes_Fme07::reset()
{
	last_time = 0;
	memset( & last_amp, 0, sizeof( last_amp ) );

	RNG = 1;
	OutputA = 0;
	OutputB = 0;
	OutputC = 0;
	OutputN = 0xff;
	for ( register_latch = 0; register_latch <= AY_ESHAPE ; ++register_latch )
		write_data( 0, 0 );
	register_latch = 0;
}

void Nes_Fme07::volume( double v )
{
	synth.volume( v * 0.1318359375 * 2 ); // ?
}

void Nes_Fme07::treble_eq( blip_eq_t const& eq )
{
	synth.treble_eq( eq );
}

void Nes_Fme07::output( Blip_Buffer* buf )
{
	for ( int i = 0; i < osc_count; i++ )
		osc_output( i, buf );
}

void Nes_Fme07::end_frame( nes_time_t time )
{
	if ( time > last_time )
		run_until( time );
	last_time -= time;
	assert( last_time >= 0 );
}

void Nes_Fme07::save_snapshot( ay_snapshot_t* out ) const
{
	out->register_latch = register_latch;

	for ( int i = 0; i < 16; ++i )
		out->Regs[ i ] = Regs[ i ];

	out->PeriodA = PeriodA;
	out->PeriodB = PeriodB;
	out->PeriodC = PeriodC;
	out->PeriodN = PeriodN;
	out->PeriodE = PeriodE;

	out->CountA = CountA;
	out->CountB = CountB;
	out->CountC = CountC;
	out->CountN = CountN;
	out->CountE = CountE;

	out->VolA = VolA;
	out->VolB = VolB;
	out->VolC = VolC;
	out->VolE = VolE;

	out->EnvelopeA = EnvelopeA;
	out->EnvelopeB = EnvelopeB;
	out->EnvelopeC = EnvelopeC;

	out->OutputA = OutputA;
	out->OutputB = OutputB;
	out->OutputC = OutputC;

	out->CountEnv = CountEnv;

	out->Hold = Hold;
	out->Alternate = Alternate;
	out->Attack = Attack;
	out->Holding = Holding;

	out->RNG = RNG;
}

void Nes_Fme07::load_snapshot( ay_snapshot_t const& in )
{
	reset();

	register_latch = in.register_latch;

	for ( int i = 0; i < 16; ++i )
		Regs[ i ] = in.Regs[ i ];

	PeriodA = in.PeriodA;
	PeriodB = in.PeriodB;
	PeriodC = in.PeriodC;
	PeriodN = in.PeriodN;
	PeriodE = in.PeriodE;

	CountA = in.CountA;
	CountB = in.CountB;
	CountC = in.CountC;
	CountN = in.CountN;
	CountE = in.CountE;

	VolA = in.VolA;
	VolB = in.VolB;
	VolC = in.VolC;
	VolE = in.VolE;

	EnvelopeA = in.EnvelopeA;
	EnvelopeB = in.EnvelopeB;
	EnvelopeC = in.EnvelopeC;

	OutputA = in.OutputA;
	OutputB = in.OutputB;
	OutputC = in.OutputC;

	CountEnv = in.CountEnv;

	Hold = in.Hold;
	Alternate = in.Alternate;
	Attack = in.Attack;
	Holding = in.Holding;

	RNG = in.RNG;
}

void Nes_Fme07::write_reg( int data )
{
	register_latch = data;
}

void Nes_Fme07::write_data( nes_time_t time, int data )
{
	run_until( time );

	int old;

	Regs[ register_latch ] = data;

	/* A note about the period of tones, noise and envelope: for speed reasons,*/
	/* we count down from the period to 0, but careful studies of the chip     */
	/* output prove that it instead counts up from 0 until the counter becomes */
	/* greater or equal to the period. This is an important difference when the*/
	/* program is rapidly changing the period to modulate the sound.           */
	/* To compensate for the difference, when the period is changed we adjust  */
	/* our internal counter.                                                   */
	/* Also, note that period = 0 is the same as period = 1. This is mentioned */
	/* in the YM2203 data sheets. However, this does NOT apply to the Envelope */
	/* period. In that case, period = 0 is half as period = 1. */
	switch( register_latch )
	{
	case AY_AFINE:
	case AY_ACOARSE:
		Regs[ AY_ACOARSE ] &= 0x0f;
		old = PeriodA;
		PeriodA = ( Regs[ AY_AFINE ] + 256 * Regs[ AY_ACOARSE ] ) * STEP_SCALE;
		if ( PeriodA == 0 ) PeriodA = STEP_SCALE;
		CountA += PeriodA - old;
		if ( CountA <= 0 ) CountA = 1;
		break;
	case AY_BFINE:
	case AY_BCOARSE:
		Regs[ AY_BCOARSE ] &= 0x0f;
		old = PeriodB;
		PeriodB = ( Regs[ AY_BFINE ] + 256 * Regs[ AY_BCOARSE ] ) * STEP_SCALE;
		if ( PeriodB == 0 ) PeriodB = STEP_SCALE;
		CountB += PeriodB - old;
		if ( CountB <= 0 ) CountB = 1;
		break;
	case AY_CFINE:
	case AY_CCOARSE:
		Regs[ AY_CCOARSE ] &= 0x0f;
		old = PeriodC;
		PeriodC = ( Regs[ AY_CFINE ] + 256 * Regs[ AY_CCOARSE ] ) * STEP_SCALE;
		if ( PeriodC == 0 ) PeriodC = STEP_SCALE;
		CountC += PeriodC - old;
		if ( CountC <= 0 ) CountC = 1;
		break;
	case AY_NOISEPER:
		Regs[ AY_NOISEPER ] &= 0x1f;
		old = PeriodN;
		PeriodN = Regs[ AY_NOISEPER ] * STEP_SCALE;
		if ( PeriodN == 0 ) PeriodN = STEP_SCALE;
		CountN += PeriodN - old;
		if ( CountN <= 0 ) CountN = 1;
		break;
	case AY_AVOL:
		Regs[ AY_AVOL ] &= 0x1f;
		EnvelopeA = Regs[ AY_AVOL ] & 0x10;
		VolA = EnvelopeA ? VolE : VolTable[ Regs[ AY_AVOL ] ? Regs[ AY_AVOL ] * 2 + 1 : 0 ];
		break;
	case AY_BVOL:
		Regs[ AY_BVOL ] &= 0x1f;
		EnvelopeB = Regs[ AY_BVOL ] & 0x10;
		VolB = EnvelopeB ? VolE : VolTable[ Regs[ AY_BVOL ] ? Regs[ AY_BVOL ] * 2 + 1 : 0 ];
		break;
	case AY_CVOL:
		Regs[ AY_CVOL ] &= 0x1f;
		EnvelopeC = Regs[ AY_CVOL ] & 0x10;
		VolC = EnvelopeC ? VolE : VolTable[ Regs[ AY_CVOL ] ? Regs[ AY_CVOL ] * 2 + 1 : 0 ];
		break;
	case AY_EFINE:
	case AY_ECOARSE:
		old = PeriodE;
		PeriodE = ( ( Regs[ AY_EFINE ] + 256 * Regs[ AY_ECOARSE ] ) ) * STEP_SCALE;
		if ( PeriodE == 0 ) PeriodE = STEP_SCALE / 2;
		CountE += PeriodE - old;
		if ( CountE <= 0 ) CountE = 1;
		break;
	case AY_ESHAPE:
		/* envelope shapes:
        C AtAlH
        0 0 x x  \___

        0 1 x x  /___

        1 0 0 0  \\\\

        1 0 0 1  \___

        1 0 1 0  \/\/
                  ___
        1 0 1 1  \

        1 1 0 0  ////
                  ___
        1 1 0 1  /

        1 1 1 0  /\/\

        1 1 1 1  /___

        The envelope counter on the AY-3-8910 has 16 steps. On the YM2149 it
        has twice the steps, happening twice as fast. Since the end result is
        just a smoother curve, we always use the YM2149 behaviour.
        */
		Regs[ AY_ESHAPE ] &= 0x0f;
		Attack = ( Regs[ AY_ESHAPE ] & 0x04 ) ? 0x1f : 0x00;
		if ( ( Regs[ AY_ESHAPE ] & 0x08 ) == 0 )
		{
			/* if Continue = 0, map the shape to the equivalent one which has Continue = 1 */
			Hold = 1;
			Alternate = Attack;
		}
		else
		{
			Hold = Regs[ AY_ESHAPE ] & 0x01;
			Alternate = Regs[ AY_ESHAPE ] & 0x02;
		}
		CountE = PeriodE;
		CountEnv = 0x1f;
		Holding = 0;
		VolE = VolTable[ CountEnv ^ Attack ];
		if ( EnvelopeA ) VolA = VolE;
		if ( EnvelopeB ) VolB = VolE;
		if ( EnvelopeC ) VolC = VolE;
		break;
	}
}

void Nes_Fme07::run_until( nes_time_t end_time )
{
	int outn;

	nes_time_t time = last_time;
	int length = end_time - last_time;

	int amp, delta;

	/* The 8910 has three outputs, each output is the mix of one of the three */
	/* tone generators and of the (single) noise generator. The two are mixed */
	/* BEFORE going into the DAC. The formula to mix each channel is: */
	/* (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable). */
	/* Note that this means that if both tone and noise are disabled, the output */
	/* is 1, not 0, and can be modulated changing the volume. */


	/* If the channels are disabled, set their output to 1, and increase the */
	/* counter, if necessary, so they will not be inverted during this update. */
	/* Setting the output to 1 is necessary because a disabled channel is locked */
	/* into the ON state (see above); and it has no effect if the volume is 0. */
	/* If the volume is 0, increase the counter, but don't touch the output. */
	if ( Regs[ AY_ENABLE ] & 0x01 )
	{
		if ( CountA <= length * STEP ) CountA += length * STEP;
		OutputA = 1;
	}
	else if ( Regs[ AY_AVOL ] == 0 )
	{
		/* note that I do count += length, NOT count = length + 1. You might think */
		/* it's the same since the volume is 0, but doing the latter could cause */
		/* interferencies when the program is rapidly modulating the volume. */
		if ( CountA <= length * STEP ) CountA += length * STEP;
	}
	if ( Regs[ AY_ENABLE ] & 0x02 )
	{
		if ( CountB <= length * STEP ) CountB += length * STEP;
		OutputB = 1;
	}
	else if ( Regs[ AY_BVOL ] == 0 )
	{
		if ( CountB <= length * STEP ) CountB += length * STEP;
	}
	if ( Regs[ AY_ENABLE ] & 0x04 )
	{
		if ( CountC <= length * STEP ) CountC += length * STEP;
		OutputC = 1;
	}
	else if ( Regs[ AY_CVOL ] == 0 )
	{
		if ( CountC <= length * STEP ) CountC += length * STEP;
	}

	amp = OutputA ? VolA : 0;
	delta = amp - last_amp[ 0 ];
	if ( delta )
	{
		last_amp[ 0 ] += delta;
		if ( out[ 0 ] ) synth.offset( time, delta, out[ 0 ] );
	}

	amp = OutputB ? VolB : 0;
	delta = amp - last_amp[ 1 ];
	if ( delta )
	{
		last_amp[ 1 ] += delta;
		if ( out[ 1 ] ) synth.offset( time, delta, out[ 1 ] );
	}

	amp = OutputC ? VolC : 0;
	delta = amp - last_amp[ 2 ];
	if ( delta )
	{
		last_amp[ 2 ] += delta;
		if ( out[ 2 ] ) synth.offset( time, delta, out[ 2 ] );
	}

	/* for the noise channel we must not touch OutputN - it's also not necessary */
	/* since we use outn. */
	if ( ( Regs[ AY_ENABLE ] & 0x38 ) == 0x38 ) /* all off */
		if ( CountN <= length * STEP ) CountN += length * STEP;

	outn = ( OutputN | Regs[ AY_ENABLE ] );


	/* buffering loop */
	while ( length )
	{
		int left = STEP;
		do
		{
			int nextevent = left;

			if ( CountN < nextevent ) nextevent = CountN;
			if ( ! Holding && CountE < nextevent ) nextevent = CountE;

			if ( outn & 0x08 )
			{
				CountA -= nextevent;
				while ( CountA <= 0 )
				{
					CountA += PeriodA;
					if ( CountA > 0 )
					{
						OutputA ^= 1;
						amp = OutputA ? VolA : 0;
						delta = amp - last_amp[ 0 ];
						if ( delta )
						{
							last_amp[ 0 ] += delta;
							if ( out[ 0 ] ) synth.offset( time, delta, out[ 0 ] );
						}
					}
				}
			}
			else
			{
				CountA -= nextevent;
				while ( CountA <= 0 )
				{
					CountA += PeriodA;
					if ( CountA > 0 )
					{
						OutputA ^= 1;
					}
				}
			}

			if ( outn & 0x10 )
			{
				CountB -= nextevent;
				while ( CountB <= 0 )
				{
					CountB += PeriodB;
					if ( CountB > 0 )
					{
						OutputB ^= 1;
						amp = OutputB ? VolB : 0;
						delta = amp - last_amp[ 1 ];
						if ( delta )
						{
							last_amp[ 1 ] += delta;
							if ( out[ 1 ] ) synth.offset( time, delta, out[ 1 ] );
						}
					}
				}
			}
			else
			{
				CountB -= nextevent;
				while ( CountB <= 0 )
				{
					CountB += PeriodB;
					if ( CountB > 0 )
					{
						OutputB ^= 1;
					}
				}
			}

			if ( outn & 0x20 )
			{
				CountC -= nextevent;
				while ( CountC <= 0 )
				{
					CountC += PeriodC;
					if ( CountC > 0 )
					{
						OutputC ^= 1;
						amp = OutputC ? VolC : 0;
						delta = amp - last_amp[ 2 ];
						if ( delta )
						{
							last_amp[ 2 ] += delta;
							if ( out[ 2 ] ) synth.offset( time, delta, out[ 2 ] );
						}
					}
				}
			}
			else
			{
				CountC -= nextevent;
				while ( CountC <= 0 )
				{
					CountC += PeriodC;
					if ( CountC > 0 )
					{
						OutputC ^= 1;
					}
				}
			}

			CountN -= nextevent;
			if ( CountN <= 0 )
			{
				/* Is noise output going to change? */
				if ( ( RNG + 1 ) & 2 )	/* (bit0^bit1)? */
				{
					OutputN = ~OutputN;
					outn = ( OutputN | Regs[ AY_ENABLE ] );
				}

				/* The Random Number Generator of the 8910 is a 17-bit shift */
				/* register. The input to the shift register is bit0 XOR bit3 */
				/* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */

				/* The following is a fast way to compute bit17 = bit0^bit3. */
				/* Instead of doing all the logic operations, we only check */
				/* bit0, relying on the fact that after three shifts of the */
				/* register, what now is bit3 will become bit0, and will */
				/* invert, if necessary, bit14, which previously was bit17. */
				if ( RNG & 1 ) RNG ^= 0x24000; /* This version is called the "Galois configuration". */
				RNG >>= 1;
				CountN += PeriodN;
			}

			/* update envelope */
			if ( Holding == 0 )
			{
				CountE -= nextevent;
				if ( CountE <= 0 )
				{
					do
					{
						CountEnv--;
						CountE += PeriodE;
					}
					while ( CountE <= 0 );

					/* check envelope current position */
					if ( CountEnv < 0 )
					{
						if ( Hold )
						{
							if ( Alternate )
								Attack ^= 0x1f;
							Holding = 1;
							CountEnv = 0;
						}
						else
						{
							/* if CountEnv has looped an odd number of times (usually 1), */
							/* invert the output. */
							if ( Alternate && ( CountEnv & 0x20 ) )
 								Attack ^= 0x1f;

							CountEnv &= 0x1f;
						}
					}

					VolE = VolTable[ CountEnv ^ Attack ];
					/* reload volume */
					if ( EnvelopeA ) VolA = VolE;
					if ( EnvelopeB ) VolB = VolE;
					if ( EnvelopeC ) VolC = VolE;
				}
			}

			left -= nextevent;
		}
		while ( left > 0 );

		--length;
		++time;
	}

	last_time = end_time;
}
