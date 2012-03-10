
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Recorder.h"

#include <stdlib.h>

/* Copyright (C) 2004-2005 Shay Green. This module is free software; you
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

Nes_Recorder::Nes_Recorder( frame_count_t snapshot_period )
{
	film = NULL;
	pixels = NULL;
	sound_buf = NULL;
	channel_count_ = 0;
	default_sound_buf = NULL;
	circular_duration = 5 * 60 * frames_per_second; // default
	movie_resync_enabled = false;
	default_film.clear( snapshot_period ? snapshot_period : 60 * frames_per_second );
	equalizer_ = equalizer_t( -8.87, 8800, 90 );
}

Nes_Recorder::~Nes_Recorder()
{
	delete default_sound_buf;
}

blargg_err_t Nes_Recorder::init()
{
	if ( !film )
	{
		BLARGG_RETURN_ERR( emu::init() );
		
		frame_count_t const cache_duration = 3 * 60 * frames_per_second; // configurable
		
		// the +1 might reduce cache thrashing
		BLARGG_RETURN_ERR( cache.resize( cache_duration / cache_period_ + 1 ) );
		
		film = &default_film;
		BLARGG_RETURN_ERR( reapply_circularity() );
	}
	
	return blargg_success;
}

blargg_err_t Nes_Recorder::use_circular_film( frame_count_t count )
{
	circular_duration = count;
	return reapply_circularity();
}

blargg_err_t Nes_Recorder::reapply_circularity()
{
	if ( film == &default_film )
	{
		if ( default_film.empty() )
			default_film.clear(); // clear circularity
		
		if ( circular_duration > 0 )
			BLARGG_RETURN_ERR( default_film.make_circular( circular_duration ) );
	}
	return blargg_success;
}

blargg_err_t Nes_Recorder::set_sample_rate( long rate, Multi_Buffer* new_buf )
{
	if ( sound_buf != new_buf )
	{
		sound_buf = new_buf;
		BLARGG_RETURN_ERR( emu::init() );
		emu::get_apu().volume( 1.0 ); // cancel any previous non-linearity
	}
	BLARGG_RETURN_ERR( sound_buf->sample_rate( rate, 1000 / frames_per_second ) );
	sound_buf->clock_rate( sound_clock_rate );
	if ( sound_buf != default_sound_buf )
	{
		delete default_sound_buf;
		default_sound_buf = NULL;
	}
	return blargg_success;
}

blargg_err_t Nes_Recorder::set_sample_rate( long rate )
{
	if ( !default_sound_buf )
	{
		default_sound_buf = BLARGG_NEW Mono_Buffer;
		BLARGG_CHECK_ALLOC( default_sound_buf );
	}
	return set_sample_rate( rate, default_sound_buf );
}

void Nes_Recorder::set_equalizer( const equalizer_t& eq )
{
	equalizer_ = eq;
	if ( rom.loaded() )
	{
		blip_eq_t blip_eq( eq.treble, eq.cutoff, sound_buf->sample_rate() );
		emu::get_apu().treble_eq( blip_eq );
		emu::get_mapper().set_treble( blip_eq );
		sound_buf->bass_freq( equalizer_.bass );
	}
}

blargg_err_t Nes_Recorder::load_ines_rom( Data_Reader& in, Data_Reader* ips )
{
	// auto init
	BLARGG_RETURN_ERR( init() );
	
	// optionally patch while loading
	if ( ips )
		BLARGG_RETURN_ERR( rom.load_patched_ines_rom( in, *ips ) );
	else
		BLARGG_RETURN_ERR( rom.load_ines_rom( in ) );
	
	BLARGG_RETURN_ERR( emu::open_rom( &rom ) );
	
	// assign sound channels
	channel_count_ = emu::get_apu().osc_count + emu::get_mapper().channel_count();
	
	BLARGG_RETURN_ERR( sound_buf->set_channel_count( channel_count() ) );
	set_equalizer( equalizer_ );
	enable_sound( true );
	
	reset();
	
	return blargg_success;
}

void Nes_Recorder::enable_sound( bool enabled, int index )
{
	int end = index + 1;
	if ( index < 0 )
	{
		index = 0;
		end = channel_count();
	}
	
	for ( ; index < end; index++ )
	{
		require( (unsigned) index < channel_count() );
		
		Blip_Buffer* buf = (enabled ? sound_buf->channel( index ).center : NULL);
		int mapper_index = index - emu::get_apu().osc_count;
		if ( mapper_index < 0 )
			emu::get_apu().osc_output( index, buf );
		else
			emu::get_mapper().set_channel_buf( mapper_index, buf );
	}
}

void Nes_Recorder::clear_sound_buf()
{
	sound_buf_end = 0;
	sound_buf->clear();
	emu::get_apu().buffer_cleared();
	fade_sound_out = false;
	fade_sound_in = true;
}

void Nes_Recorder::reset( bool full_reset, bool erase_battery_ram )
{
	require( rom.loaded() );
	
	skip_next_frame = false;
	clear_sound_buf();
	clear_cache();
	
	emu::reset( full_reset, erase_battery_ram );
	film->clear();
}

void Nes_Recorder::clear_cache()
{
	for ( int i = 0; i < cache.size(); i++ )
		cache [i].set_timestamp( invalid_frame_count );
}

void Nes_Recorder::set_film( Nes_Film* new_film, frame_count_t time )
{
	require( rom.loaded() );
	
	// to do: reduce complexity
	
	if ( !new_film )
	{
		new_film = &default_film;
		default_film.clear();
	}
	else if ( new_film != &default_film )
	{
		default_film.clear();
	}
	
	film = new_film;
	if ( !film->empty() )
	{
		emu::set_timestamp( invalid_frame_count );
		clear_cache();
		seek( time );
	}
}

void Nes_Recorder::set_film( Nes_Film* new_film )
{
	set_film( new_film, new_film ? new_film->begin() : default_film.begin() );
}

// Non-core functionality

void Nes_Recorder::new_film()
{
	default_film.clear();
	set_film( &default_film );
}

blargg_err_t Nes_Recorder::load_movie( Data_Reader& in )
{
	BLARGG_RETURN_ERR( default_film.read( in ) );
	BLARGG_RETURN_ERR( reapply_circularity() );
	set_film( &default_film );
	return blargg_success;
}

blargg_err_t Nes_Recorder::load_battery_ram( Data_Reader& in )
{
	check( film->empty() );
	return emu::load_battery_ram( in );
}

blargg_err_t Nes_Recorder::load_snapshot( Data_Reader& in )
{
	Nes_Snapshot_Array snapshots;
	BLARGG_RETURN_ERR( snapshots.resize( 1 ) );
	BLARGG_RETURN_ERR( snapshots [0].read( in ) );
	load_snapshot( snapshots [0] );
	return blargg_success;
}

void Nes_Recorder::load_snapshot( Nes_Snapshot const& in )
{
	reset();
	load_snapshot_( in );
}

blargg_err_t Nes_Recorder::save_snapshot( Data_Writer& out ) const
{
	Nes_Snapshot_Array snapshots;
	BLARGG_RETURN_ERR( snapshots.resize( 1 ) );
	save_snapshot( &snapshots [0] );
	return snapshots [0].write( out );
}

blargg_err_t Nes_Recorder::save_movie( Data_Writer& out, frame_count_t period ) const
{
	return film->write( out, period, film->begin(), film->end() );
}

void Nes_Recorder::quick_seek( frame_count_t time )
{
	// don't adjust time if seeking to beginning or end
	if ( time != film->begin() && time != film->end() )
	{
		// rounded time must be within about a minute of requested time
		int const half_threshold = 45 * frames_per_second;
		
		// find nearest snapshots before and after requested time
		frame_count_t after = invalid_frame_count;
		frame_count_t before = time + half_threshold;
		while ( time < before )
		{
			after = before;
			Nes_Snapshot const* ss = nearest_snapshot( film->constrain( before - 1 ) );
			if ( !ss )
			{
				require( false ); // tried to seek outside movie
				return;
			}
			before = ss->timestamp();
		}
		
		// deterime which is closer
		frame_count_t closest = after;
		if ( time - before < after - time )
			closest = before;
		
		// use if close enough
		if ( abs( time - closest ) < half_threshold )
			time = closest;
	}
	seek( time );
}

void Nes_Recorder::skip( int delta )
{
	if ( delta ) // rounding code can't handle zero
	{
		// round to nearest cache timestamp (even if not in cache)
		frame_count_t current = tell();
		frame_count_t time = current + delta + cache_period_ / 2;
		time -= time % cache_period_;
		if ( delta < 0 )
		{
			if ( time >= current )
				time -= cache_period_;
		}
		else if ( time <= current )
		{
			time += cache_period_;
		}
		seek( film->constrain( time ) );
	}
}

// Sound

void Nes_Recorder::fade_samples_( blip_sample_t* p, int size, int step )
{
	if ( size >= fade_size_ )
	{
		if ( step < 0 )
			p += size - fade_size_;
		
		const int shift = 15;
		int mul = (1 - step) << (shift - 1);
		step *= (1 << shift) / fade_size_;
		
		for ( int n = fade_size_; n--; )
		{
			*p = (*p * mul) >> 15;
			++p;
			mul += step;
		}
	}
}

long Nes_Recorder::read_samples( short* out, long out_size )
{
	sound_buf_end = 0; // samples in buffer have now been read
	long count = sound_buf->read_samples( out, out_size );
	if ( fade_sound_in )
	{
		fade_sound_in = false;
		fade_samples_( out, count, 1 );
	}
	
	if ( fade_sound_out )
	{
		fade_sound_out = false;
		fade_sound_in = true; // next buffer should be faded in
		fade_samples_( out, count, -1 );
	}
	return count;
}

// Frame emulation

void Nes_Recorder::load_snapshot_( Nes_Snapshot const& ss )
{
	skip_next_frame = false;
	clear_sound_buf(); // to do: optimize this
	return emu::load_snapshot( ss );
}

inline int Nes_Recorder::cache_index( frame_count_t t ) const
{
	return (t / cache_period_) % cache.size();
}

// emulate frame and cache snapshot
void Nes_Recorder::emulate_frame_( joypad_t joypad, bool playing, bool rendering )
{
	// add snapshot to movie if there is none for this timestamp and it's aligned
	Nes_Snapshot& ss = film->get_snapshot( emu::timestamp() );
	if ( ss.timestamp() == invalid_frame_count && emu::timestamp() % film->period() == 0 )
		emu::save_snapshot( &ss );
	
	// update cache
	if ( emu::timestamp() % cache_period_ == 0 )
		save_snapshot( &cache [cache_index( emu::timestamp() )] );
	
	if ( playing && skip_next_frame )
	{
		skip_next_frame = false;
		frames_emulated_ = 0;
		emu::set_timestamp( emu::timestamp() + 1 );
	}
	else
	{
		skip_next_frame = false;
		
		bool joypad_needed = (joypad & 0xff) != 0xff;
		
		if ( joypad_needed || !film->has_joypad_sync() )
			last_joypad = joypad;
		else if ( film->contains_frame( emu::timestamp() + 1 ) )
			last_joypad = film->get_joypad( emu::timestamp() + 1 );
		
		frames_emulated_ = 1;
		sound_buf_end = emu::emulate_frame( last_joypad & 0xff, (last_joypad >> 8) & 0xff );
		emu::set_pixels( NULL, 0 );
		
		bool joypads_read = (emu::joypad_read_count() != 0);
		if ( playing && film->has_joypad_sync() && joypad_needed != joypads_read )
		{
			check( false ); // resync should be very rare, so make note when it occurs
			
			skip_next_frame = joypads_read;
			if ( !skip_next_frame )
			{
				if ( rendering )
					clear_sound_buf();
				
				frames_emulated_ = 2;
				sound_buf_end = emu::emulate_frame( last_joypad & 0xff, (last_joypad >> 8) & 0xff );
				
				check( emu::joypad_read_count() );
				
				emu::set_timestamp( emu::timestamp() - 1 );
			}
		}
	}
}

// emulate and render frame
void Nes_Recorder::render_frame_( joypad_t joypad, bool playing )
{
	require( film );
	
	if ( pixels )
		emu::set_pixels( pixels, row_bytes );
	
	if ( sound_buf_end )
		clear_sound_buf(); // most recently skipped frames so buffer contains garbage
	
	emulate_frame_( joypad, playing, true );
	sound_buf->end_frame( sound_buf_end, false );
}

// replay and render frame
void Nes_Recorder::play_frame_()
{
	render_frame_( film->get_joypad( emu::timestamp() ), true );
	
	if ( movie_resync_enabled )
	{
		Nes_Snapshot& ss = film->get_snapshot( emu::timestamp() );
		if ( ss.timestamp() == emu::timestamp() )
		{
			fade_sound_out = true;
			load_snapshot_( ss );
		}
	}
}

// Movie handling

// to do: find way to make this non-virtual
frame_count_t Nes_Recorder::tell() const
{
	return film->constrain( emu::timestamp() );
}

Nes_Snapshot const* Nes_Recorder::nearest_snapshot( frame_count_t time ) const
{
	Nes_Snapshot const* ss = film->nearest_snapshot( time );
	if ( ss )
	{
		// check cache for any snapshots more recent than film's
		for ( frame_count_t t = time - time % cache_period_; ss->timestamp() < t;
				t -= cache_period_ )
		{
			Nes_Snapshot const& cache_ss = cache [cache_index( t )];
			if ( cache_ss.timestamp() == t )
			{
				ss = &cache_ss;
				break;
			}
		}
	}
	return ss;
}

void Nes_Recorder::seek_( frame_count_t time )
{
	Nes_Snapshot const* ss = nearest_snapshot( time );
	if ( !film->contains( time ) || !ss )
	{
		require( false ); // tried to seek outside recording
		return;
	}
	
	// restore snapshot and emulate until desired time
	load_snapshot_( *ss );
	while ( emu::timestamp() < time )
		emulate_frame_( film->get_joypad( emu::timestamp() ), true );
	
	assert( emu::timestamp() == time );
}

void Nes_Recorder::seek( frame_count_t time )
{
	if ( time != tell() )
		seek_( time );
}

blargg_err_t Nes_Recorder::next_frame( int joypad, int joypad2 )
{
	if ( film->contains_frame( tell() ) )
	{
		// playing
		play_frame_();
	}
	else
	{
		// recording
		require( joypad >= 0 ); // -1 is used when caller expects playback
		
		// prepare/check film
		frame_count_t time = emu::timestamp();
		if ( !film->empty() )
			require( film->contains( time ) );
		else if ( film == &default_film )
			BLARGG_RETURN_ERR( reapply_circularity() );
		
		joypad_t joypads = joypad2 * 0x100 + joypad;
		
		// record frame
		Nes_Snapshot* ss = NULL;
		BLARGG_RETURN_ERR( film->record_frame( time, joypads, &ss ) );
		if ( ss )
			save_snapshot( ss );
		
		render_frame_( joypads, false );
		
		// if joypad wasn't even read this frame, mark that in movie with the value 0xff
		if ( film->has_joypad_sync() && !emu::joypad_read_count() )
			BLARGG_RETURN_ERR( film->record_frame( time, 0xff ) );
	}
	
	return blargg_success;
}

