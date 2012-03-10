
// Nes_Emu 0.5.4. http://www.slack.net/~ant/

#include "Nes_Film.h"

#include <string.h>
#include <stdlib.h>
#include "blargg_endian.h"

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

Nes_Film::Nes_Film( frame_count_t p )
{
	for ( int i = 0; i < max_joypads; i++ )
		joypad_data [i] = NULL;
	clear( p );
}

Nes_Film::~Nes_Film()
{
	clear();
}

void Nes_Film::clear( frame_count_t new_period )
{
	period_ = new_period;
	offset = 0;
	begin_ = 0;
	end_ = 0;
	has_joypad_sync_ = true;
	
	for ( int i = 0; i < max_joypads; i++ )
	{
		free( joypad_data [i] );
		joypad_data [i] = NULL;
	}
	circular_len = 0;
	!snapshots.resize( 0 );
}

frame_count_t Nes_Film::constrain( frame_count_t t ) const
{
	if ( t != invalid_frame_count && !empty() )
	{
		if ( t < begin_ )
			t = begin_;
		
		if ( t > end_ )
			t = end_;
	}
	return t;
}

bool Nes_Film::contains( frame_count_t t ) const
{
	return !empty() && begin() <= t && t <= end();
}

inline bool Nes_Film::contains_range( frame_count_t first, frame_count_t last ) const
{
	return begin_ <= first && first <= last && last <= end_;
}

blargg_err_t Nes_Film::make_circular( frame_count_t duration )
{
	// to do: way to catch second call, since film loop won't be expanded
	if ( empty() || !circular_len )
	{
		if ( duration < length() )
			duration = length();
		
		frame_count_t extra = 0;
		if ( snapshots.size() > 0 )
		{
			int count = 0;
			for ( int i = snapshot_index( end_ - 1) + 1; i--; )
			{
				if ( snapshots [i].timestamp() == invalid_frame_count )
				{
					count++;
				}
				else
				{
					if ( extra < count )
						extra = count;
					count = 0;
				}
			}
		}
		
		dprintf( "Largest run of invalid snapshots: %d\n", (int) extra );
		
		extra = (extra + 1) * period_;
		circular_len = capacity_() - extra;
		if ( circular_len < duration )
		{
			BLARGG_RETURN_ERR( resize_( duration + extra, period_ ) );
			circular_len = duration;
		}
		
		frame_count_t old_begin = begin_;
		
		trim_circular();
		assert( begin_ == old_begin );
	}
	
	return blargg_success;
}

blargg_err_t Nes_Film::resize_joypad_data( int index, frame_count_t period )
{
	long size = joypad_history_size();
	void* new_joypad = realloc( joypad_data [index], size );
	BLARGG_CHECK_ALLOC( new_joypad );
	if ( !joypad_data [index] )
		memset( new_joypad, 0, size );
	joypad_data [index] = (BOOST::uint8_t*) new_joypad;
	return blargg_success;
}

blargg_err_t Nes_Film::resize_( frame_count_t duration, frame_count_t new_period )
{
	BLARGG_RETURN_ERR( snapshots.resize( (duration + new_period - 1) / new_period ) );
	
	for ( int i = 0; i < max_joypads; i++ )
	{
		if ( joypad_data [i] || i == 0 ) // always allocate first joypad
			BLARGG_RETURN_ERR( resize_joypad_data( i, new_period ) );
	}
	
	period_ = new_period;
	
	return blargg_success;
}

blargg_err_t Nes_Film::reserve( frame_count_t duration )
{
	duration += begin_ - offset;
	if ( !circular_len && capacity_() < duration )
		return resize_( duration, period_ );
	
	return blargg_success;
}

void Nes_Film::trim_circular()
{
	frame_count_t earliest = end_ - (circular_len ? circular_len : capacity_());
	if ( begin_ < earliest )
	{
		require( circular_len ); // non-circular, so there must be space
		begin_ = earliest;
		circular_len = end_ - begin_; // make circular if it wasn't already
	}
}

blargg_err_t Nes_Film::record_frame( frame_count_t time, joypad_t joypad, Nes_Snapshot** out )
{
	if ( out )
		*out = NULL;
	
	// make space for new frame
	frame_count_t needed = time + 1 - begin();
	if ( !contains( time ) )
	{
		require( empty() );
		offset = time - time % period_;
		begin_ = time;
		end_ = time;
		needed = 1;
	}
	if ( !circular_len )
		BLARGG_RETURN_ERR( reserve( needed + needed / 4 + 8 ) );
	else if ( begin_ <= time - circular_len )
		begin_ = time + 1 - circular_len;
	
	end_ = time + 1;
	
	long index = joypad_index( time );
	//dprintf( "Record joypad index %d\n", index );
	for ( int i = 0; i < max_joypads; i++, joypad >>= 8 )
	{
		if ( (joypad & 0xff) && !joypad_data [i] )
		{
			dprintf( "Adding joypad #%d track\n", i + 1 );
			BLARGG_RETURN_ERR( resize_joypad_data( i, period() ) );
		}
		if ( joypad_data [i] )
			joypad_data [i] [index] = joypad & 0xff;
	}
	
	if ( out && (time == begin_ || time % period_ == 0) )
		*out = &snapshots [snapshot_index( time )];
	
	return blargg_success;
}

Nes_Snapshot const* Nes_Film::nearest_snapshot( frame_count_t time ) const
{
	require( contains( time ) );
	
	if ( time >= end() )
		time = end() - 1;
	
	int index = snapshot_index( time );
	int first_index = 0;
	if ( circular_len ) // to do: is this the correct index? off by one?
		first_index = (index + 1) % snapshots.size();
	while ( true )
	{
		Nes_Snapshot const& ss = snapshots [index];
		if ( ss.timestamp() <= time ) // invalid timestamp will always be greater
			return &ss;
		
		if ( index == first_index )
			break;
		
		index = (index + snapshots.size() - 1) % snapshots.size();
	}
	
	return NULL;
}

void Nes_Film::trim( frame_count_t first, frame_count_t last )
{
	if ( contains_range( first, last ) )
	{
		begin_ = first;
		end_ = last;
		// if made empty by trimming out all contents, won't be completely clear
		check( !empty() );
	}
	else
	{
		require( false ); // tried to trim outside recording
	}
}

Nes_Film::joypad_t Nes_Film::get_joypad( frame_count_t t ) const
{
	// to do: this won't catch all bad times before beginning
	assert( end_ - capacity_() <= t && t < end_ );
	long index = joypad_index( t );
	joypad_t result = 0;
	//dprintf( "Read joypad index %d\n", index );
	for ( int i = 0; i < max_joypads; i++ )
	{
		if ( joypad_data [i] )
			result |= joypad_data [i] [index] << (i * 8);
	}
	return result;
}

// Read/write

nes_tag_t const joypad_data_tag = 'JOYP';

blargg_err_t Nes_Film_Writer::end( Nes_Film const& film, frame_count_t period,
		frame_count_t first, frame_count_t last )
{
	BLARGG_RETURN_ERR( film.write_blocks( *this, period, first, last ) );
	return Nes_File_Writer::end();
}

blargg_err_t Nes_Film::write( Data_Writer& out, frame_count_t period,
		frame_count_t first, frame_count_t last ) const
{
	Nes_Film_Writer writer;
	BLARGG_RETURN_ERR( writer.begin( &out ) );
	return writer.end( *this, period, first, last );
}

static blargg_err_t write_snapshot( Nes_Snapshot const& ss, Nes_File_Writer& out )
{
	BLARGG_RETURN_ERR( out.begin_group( snapshot_file_tag ) );
	BLARGG_RETURN_ERR( ss.write_blocks( out ) );
	return out.end_group();
}

blargg_err_t Nes_Film::write_blocks( Nes_File_Writer& out, frame_count_t period,
		frame_count_t first, frame_count_t last ) const
{
	require( contains_range( first, last ) );
	
	// find first snapshot that recording will begin with
	Nes_Snapshot const* first_snapshot = nearest_snapshot( first );
	require( first_snapshot );
	assert( first_snapshot->timestamp() <= first );
	
	// write info block
	movie_info_t info;
	memset( &info, 0, sizeof info );
	info.begin = first;
	info.length = last - first;
	info.extra = first - first_snapshot->timestamp();
	info.period = period;
	info.joypad_count = (joypad_data [1] ? 2 : 1); // to do: only handles one or two joypads
	info.has_joypad_sync = has_joypad_sync_;
	
	first -= info.extra;
	frame_count_t length = last - first;
	BLARGG_RETURN_ERR( write_nes_state( out, info ) );
	
	// write joypad data, possibly in two chunks if it wraps around
	for ( int i = 0; i < max_joypads; i++ )
	{
		byte const* data = joypad_data [i];
		if ( data )
		{
			BLARGG_RETURN_ERR( out.write_block( joypad_data_tag, length ) );
			long index = joypad_index( first );
			long count = joypad_history_size() - index;
			if ( count > length )
				count = length;
			
			BLARGG_RETURN_ERR( out.write( data + index, count ) );
			
			if ( count < length )
				BLARGG_RETURN_ERR( out.write( data, length - count ) );
		}
	}
	
	// first snapshot
	BLARGG_RETURN_ERR( write_snapshot( *first_snapshot, out ) );
	
	// write any valid snapshots in chronological order
	Nes_Snapshot const* last_snapshot = first_snapshot;
	for ( frame_count_t t = first + period; t < last; t += period )
	{
		Nes_Snapshot const* ss = nearest_snapshot( t );
		if ( ss && ss != last_snapshot )
		{
			last_snapshot = ss;
			BLARGG_RETURN_ERR( write_snapshot( *ss, out ) );
		}
	}
	
	return blargg_success;
}

// read

blargg_err_t Nes_Film::read( Data_Reader& in )
{
	Nes_Film_Reader reader;
	BLARGG_RETURN_ERR( reader.begin( &in, this ) );
	while ( !reader.done() )
		BLARGG_RETURN_ERR( reader.next_block() );
	return blargg_success;
}

Nes_Film_Reader::Nes_Film_Reader()
{
	film = NULL;
	info_ptr = NULL;
	joypad_count = 0;
	memset( &info_, 0, sizeof info_ );
}

Nes_Film_Reader::~Nes_Film_Reader()
{
}

blargg_err_t Nes_Film_Reader::begin( Data_Reader* dr, Nes_Film* nf )
{
	film = nf;
	film->clear();
	BLARGG_RETURN_ERR( Nes_File_Reader::begin( dr ) );
	if ( block_tag() != movie_file_tag )
		return "Not a movie file";
	return blargg_success;
}

blargg_err_t Nes_Film_Reader::customize( frame_count_t period )
{
	require( info_ptr );
	if ( film->snapshots.size() > 0 )
		return blargg_success;
	
	return film->begin_read( *this, info_, period );
}

blargg_err_t Nes_Film_Reader::next_block()
{
	while ( true )
	{
		BLARGG_RETURN_ERR( Nes_File_Reader::next_block() );
		switch ( depth() == 0 ? block_tag() : 0 )
		{
			case info_.tag:
				check( !info_ptr );
				BLARGG_RETURN_ERR( read_nes_state( *this, &info_ ) );
				info_ptr = &info_;
				return blargg_success;
			
			case joypad_data_tag:
				BLARGG_RETURN_ERR( customize() );
				BLARGG_RETURN_ERR( film->read_joypad( *this, joypad_count++ ) );
				break;
			
			case snapshot_file_tag: // should work fine as long as first snapshot is present
				BLARGG_RETURN_ERR( customize() );
				BLARGG_RETURN_ERR( film->read_snapshot( *this ) );
				break;
			
			default:
				if ( done() )
				{
					// at least first snapshot must have been read
					check( film->snapshots [0].timestamp() != invalid_frame_count );
					film->begin_ += info_.extra;
				}
				return blargg_success;
		}
	}
}

blargg_err_t Nes_Film::begin_read( Nes_File_Reader& in, movie_info_t const& info, int new_period )
{
	begin_ = info.begin;
	end_   = begin_ + info.length;
	has_joypad_sync_ = info.has_joypad_sync;
	
	begin_ -= info.extra;
	offset = begin_ - begin_ % new_period;
	BLARGG_RETURN_ERR( resize_( end_ - offset, new_period ) );
	
	assert( info.length + info.extra <= capacity_() + period_ );
	assert( begin_ < end_ );
	
	return blargg_success;
}

blargg_err_t Nes_Film::read_joypad( Nes_File_Reader& in, int index )
{
	if ( index >= max_joypads )
	{
		check( false ); // ignoring extra joypads
	}
	else
	{
		BLARGG_RETURN_ERR( resize_joypad_data( index, period() ) );
		BLARGG_RETURN_ERR( in.read( joypad_data [index] + (begin() - offset), length() ) );
	}
	
	return blargg_success;
}

blargg_err_t Nes_Film::read_snapshot( Nes_File_Reader& in )
{
	BLARGG_RETURN_ERR( in.enter_group() );
	
	// read snapshot's timestamp
	nes_state_t info;
	memset( &info, 0, sizeof info );
	while ( true )
	{
		BLARGG_RETURN_ERR( in.next_block() );
		if ( in.block_tag() == info.tag )
		{
			BLARGG_RETURN_ERR( read_nes_state( in, &info ) );
			break;
		}
		check( false ); // shouldn't encounter any unknown blocks
	}
	frame_count_t time = info.frame_count;
	
	if ( !contains( time ) )
	{
		check( false );
	}
	else
	{
		// see if new snapshot is earlier than existing snapshot for this period
		Nes_Snapshot& ss = snapshots [snapshot_index( time )];
		if ( time < ss.timestamp() )
		{
			// read new snapshot
			ss.clear();
			ss.set_nes_state( info );
			do
			{
				BLARGG_RETURN_ERR( ss.read_blocks( in ) );
			}
			while ( in.block_type() != in.group_end );
		}
	}
	
	BLARGG_RETURN_ERR( in.exit_group() );
	
	return blargg_success;
}

