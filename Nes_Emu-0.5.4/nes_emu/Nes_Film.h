
// "Film" on which to record NES movies, with "endless loop" option

// Nes_Emu 0.5.4. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_FILM_H
#define NES_FILM_H

#include "Nes_Snapshot.h"

class Nes_Film {
	enum { frames_per_second = 60 };
public:
	// One snapshot is kept for every 'period' frames of recording. A lower
	// period makes seeking faster but uses more memory. Each snapshot takes
	// about 24K of memory.
	explicit Nes_Film( frame_count_t period = 60 * frames_per_second );
	~Nes_Film();
	
	// Average number of frames between snapshots
	frame_count_t period() const;
	
	// Clear recording and make it non-circular, and optionally change period
	void clear( frame_count_t new_period );
	void clear();
	
	// Timestamp of first snapshot in recording
	frame_count_t begin() const;
	
	// Timestamp of end of last frame in recording
	frame_count_t end() const;
	
	// Duration of recorded material
	frame_count_t length() const { return end() - begin(); }
	
	// True if film has nothing recorded on it
	bool empty() const { return begin() == end(); }
	
	// True if timestamp is within recording. Always false if film is empty.
	bool contains( frame_count_t ) const;
	
	// True if recording contains frame beginning at timestamp
	bool contains_frame( frame_count_t t ) const { return begin() <= t && t < end(); }
	
	// Constrain timestamp to recorded range, or return unchanged if film is empty
	frame_count_t constrain( frame_count_t ) const;
	
	// Trim to subset of recording
	void trim( frame_count_t begin, frame_count_t end );
	
	// Turn film into an endless loop that keeps at least 'duration'
	// frames of the most recently recorded material. Current recording
	// is preserved.
	blargg_err_t make_circular( frame_count_t duration );
	// errors: out of memory
	
// File read/write
	
	// Write subset of recording to file, with snapshots approximately every 'period' frames
	blargg_err_t write( Data_Writer&, frame_count_t period,
			frame_count_t begin, frame_count_t end ) const;
	// errors: write error
	
	// Write entire recording to file
	blargg_err_t write( Data_Writer& out ) const { return write( out, period(), begin(), end() ); }
	// errors: write error
	
	// Read entire recording from file
	blargg_err_t read( Data_Reader& );
	// errors: read error
	
	// to do: add options to save movie with only initial snapshot and joypad data,
	// and possibly snapshot closest to end
	
// Raw access
	
	// True if joypad entries are 0xff on frames that the joypad isn't read
	bool has_joypad_sync() const;
	
	// Reference to snapshot that might have current timestamp
	Nes_Snapshot& get_snapshot( frame_count_t );
	
	// to do: redefine to return a reference (and thus no error condition)
	// Pointer to nearest snapshot at or before timestamp, or NULL if none
	Nes_Snapshot const* nearest_snapshot( frame_count_t ) const;
	
	// Joypad data for frame beginning at timestamp
	typedef unsigned long joypad_t;
	joypad_t get_joypad( frame_count_t ) const;
	
	// Record new frame beginning at timestamp using joypad data. Returns
	// pointer where snapshot should be saved to, or NULL if a snapshot
	// isn't needed for this timestamp.
	blargg_err_t record_frame( frame_count_t, joypad_t joypad, Nes_Snapshot** out = NULL );
	
	blargg_err_t write_blocks( Nes_File_Writer&, frame_count_t period,
		frame_count_t first, frame_count_t last ) const;
	
	// End of public interface
public:
	frame_count_t capacity_() const { return snapshots.size() * period_; }
private:
	// noncopyable
	Nes_Film( Nes_Film const& );
	Nes_Film& operator = ( Nes_Film const& );
	
	enum { max_joypads = 2 };
	BOOST::uint8_t* joypad_data [max_joypads]; // non-null if that joypad has data
	Nes_Snapshot_Array snapshots;
	frame_count_t begin_;
	frame_count_t end_;
	frame_count_t period_;
	frame_count_t offset;
	frame_count_t circular_len;
	bool has_joypad_sync_;
	
	blargg_err_t resize_joypad_data( int index, frame_count_t period );
	long joypad_history_size() const { return snapshots.size() * period_; }
	long joypad_index( frame_count_t t ) const { return (t - offset) % joypad_history_size(); }
	int snapshot_index( frame_count_t ) const;
	blargg_err_t resize_( frame_count_t duration, frame_count_t period );
	bool contains_range( frame_count_t first, frame_count_t last ) const;
	void trim_circular();
	blargg_err_t reserve( frame_count_t );
	
	blargg_err_t begin_read( Nes_File_Reader&, movie_info_t const&, int new_period );
	blargg_err_t read_joypad( Nes_File_Reader&, int index );
	blargg_err_t read_snapshot( Nes_File_Reader& );
	friend class Nes_Film_Reader;
};

class Nes_Film_Writer : public Nes_File_Writer {
public:
	blargg_err_t begin( Data_Writer* );
	blargg_err_t end( Nes_Film const& );
	blargg_err_t end( Nes_Film const&, frame_count_t period,
		frame_count_t first, frame_count_t last );
};

class Nes_Film_Reader : public Nes_File_Reader {
public:
	Nes_Film_Reader();
	~Nes_Film_Reader();
	
	blargg_err_t begin( Data_Reader*, Nes_Film* );
	blargg_err_t next_block();
	movie_info_t const* info() const { return info_ptr; }
	blargg_err_t customize( frame_count_t period );
	
private:
	Nes_Film* film;
	movie_info_t info_;
	movie_info_t* info_ptr;
	int joypad_count;
	blargg_err_t customize();
};

inline blargg_err_t Nes_Film_Reader::customize() { return customize( film->period() ); }

inline blargg_err_t Nes_Film_Writer::end( Nes_Film const& film )
{
	return end( film, film.period(), film.begin(), film.end() );
}

inline blargg_err_t Nes_Film_Writer::begin( Data_Writer* dw )
{
	return Nes_File_Writer::begin( dw, movie_file_tag );
}

inline bool Nes_Film::has_joypad_sync() const { return has_joypad_sync_; }

inline void Nes_Film::clear() { clear( period() ); }

inline frame_count_t Nes_Film::period() const { return period_; }

inline frame_count_t Nes_Film::begin() const { return begin_; }
	
inline frame_count_t Nes_Film::end() const { return end_; }
	
inline int Nes_Film::snapshot_index( frame_count_t time ) const
{
	return ((time - offset) / period_) % snapshots.size();
}

inline Nes_Snapshot& Nes_Film::get_snapshot( frame_count_t time )
{
	return snapshots [snapshot_index( time )];
}

#endif

