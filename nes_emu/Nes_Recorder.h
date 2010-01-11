
// NES video recorder that can record and play back NES game play

// Nes_Emu 0.5.0. Copyright (C) 2004-2005 Shay Green. GNU LGPL license.

#ifndef NES_RECORDER_H
#define NES_RECORDER_H

#include "Nes_Emu.h"
#include "Nes_Film.h"
#include "Multi_Buffer.h"

class Nes_Recorder : public Nes_Emu {
	typedef Nes_Emu emu;
public:
	// Optionally specify number of frames between snapshots on internal film.
	// Defaults to one every minute.
	explicit Nes_Recorder( frame_count_t snapshot_period = 0 );
	virtual ~Nes_Recorder();
	
	// See Nes_Emu.h for basic interface and reference
	
	// Set sample rate for sound generation. No quality is gained by using a rate
	// higher than 48kHz. Optionally specify a sound buffer to use instead of
	// the built-in default mono buffer.
	blargg_err_t set_sample_rate( long );
	blargg_err_t set_sample_rate_nonlinear( long );
	blargg_err_t set_sample_rate( long, Multi_Buffer* );
	// errors: out of memory
	
	// Load iNES ROM into emulator and clear recording. Optionally apply
	// IPS patch file to ROM.
	virtual blargg_err_t load_ines_rom( Data_Reader&, Data_Reader* ips = NULL );
	// errors: out of memory, read error
	
	// Reset emulated NES and clear recording
	void reset( bool full_reset = true, bool erase_battery_ram = false );
	
// Basic recording and playback

	// A movie of recent game play is always recorded
	
	// Generate frame at current timestamp. Records new frame if at end of movie,
	// otherwise replays frame and ignore 'joypad'. On exit, timestamp = timestamp + 1.
	blargg_err_t next_frame( int joypad, int joypad2 = 0 );
	// errors: out of memory
	
	// True if calling next_frame() will replay a frame from current movie
	bool replaying() const { return tell() < movie_end(); }
	
	// Trim any recorded material later than tell() so that next_frame() will
	// record next time it's called.
	void start_recording() { get_film().trim( film->begin(), tell() ); }
	
	// Skip forwards or backwards, staying within movie. May skip slightly
	// more or less than delta, but will always skip at least some if
	// delta is non-zero (i.e. repeated skip( 1 ) calls will eventually
	// reach the end of the recording then stop there).
	void skip( int delta );
	
	// Use fixed-size circular buffer of at least 'count' frames for built-in recording
	// (does not apply to load_film()). Anything already recorded or loaded will not
	// be trimmed. Passing false (0) makes any new film non-circular, but doesn't affect
	// current film unless it's empty.
	blargg_err_t use_circular_film( frame_count_t count );
	// errors: out of memory
	
	// Number of samples generated for frame
	long samples_avail() const;
	
	// Read samples for the current frame. Currently all samples must be read in
	// one call.
	long read_samples( short* out, long max_samples );
	
// Additional optional functionality
	
// Sound generation
	
	// Frequency equalizer parameters (see sound.txt)
	struct equalizer_t {
		double treble; // treble level at 22kHz, in dB (-3.0dB = 0.50)
		long cutoff;   // beginning of low-pass rolloff, in Hz
		long bass;     // high-pass breakpoint, in Hz
		equalizer_t( double treble_ = 0, long cutoff_ = 0, int bass_ = 33 ) :
				treble( treble_ ), cutoff( cutoff_ ), bass( bass_ ) { }
	};
	
	// Current frequency equalization
	const equalizer_t& equalizer() const;
	
	// Change frequency equalization
	void set_equalizer( const equalizer_t& );
	
	// Number of sound channels for current ROM
	int channel_count() const;
	
	// Enable/disable sound channel. If sound buffer's internal channel assignments
	// are changed, enable_sound( true ) should be called.
	enum { all_channels = -1 };
	void enable_sound( bool, int index = all_channels );
	
// File save/load

	// When loading a snapshot, by default the current movie is not preserved since
	// the snapshot might not be within it.
	
	// Save snapshot of current emulator state
	Nes_Emu::save_snapshot;
	blargg_err_t save_snapshot( Data_Writer& ) const;
	// errors: write error
	
	// Restore from snapshot
	void load_snapshot( Nes_Snapshot const& );
	blargg_err_t load_snapshot( Data_Reader& );
	// errors: out of memory, read error
	
	// True if current ROM claims it uses battery-backed SRAM
	bool has_battery_ram() const;
	
	// Save current battery RAM
	Nes_Emu::save_battery_ram;
	
	// Load battery RAM from file. Should only be done just after loading ROM or
	// resetting NES.
	blargg_err_t load_battery_ram( Data_Reader& );
	// errors: read error
	
	// Save current movie to a file. Optionally specify the average number of
	// frames between snapshots.
	blargg_err_t save_movie( Data_Writer&, frame_count_t period = 60 * frames_per_second ) const;
	// errors: write error
	
	// Unload current film and load movie from file
	blargg_err_t load_movie( Data_Reader& );
	// errors: out of memory, read error
	
	// When enabled, emulator is synchronized at each snapshot in movie. This
	// corrects any emulation differences compared to when the movie was made. Might
	// be necessary when playing movies made in an older version of this emulator
	// or from other emulators.
	void enable_movie_resync( bool b ) { movie_resync_enabled = b; }
	
// Film/movies
	
	// Timestamps are in terms of snapshots at a single point, rather than
	// an entire frame. Timestamps must be within current movie, that is,
	// movie_begin() <= timestamp <= movie_end().
	
	// Shown below are timestamps and the frame that occurs between 1 and 2.
	//
	// |---|///|---|---
	// 0   1   2   3    snapshots between frames
	
	// Beginning of current recording
	frame_count_t movie_begin() const { return get_film().begin(); }
	
	// End of current recording
	frame_count_t movie_end() const { return get_film().end(); }
	
	// Length of current recording
	frame_count_t movie_length() const { return get_film().length(); }
	
	// Current timestamp
	virtual frame_count_t tell() const;
	
	// Seek to new timestamp
	virtual void seek( frame_count_t t );
	
	// Seek to snapshot nearest to timestamp, which is faster than seeking to
	// an exact frame.
	void quick_seek( frame_count_t );
	
	// Unload film and start recording on new film
	void new_film();
	
	// Set film to play back or record to, or NULL to use internal film. Should be called
	// again if already-loaded film has been cleared or loaded from a file. When loaded,
	// seeks to beginning (or specified timestamp).
	void set_film( Nes_Film*, frame_count_t );
	void set_film( Nes_Film* );
	
	// Currently loaded film
	Nes_Film& get_film() const;
	
	// Number of frames between snapshots in the cache
	frame_count_t cache_period() const { return cache_period_; }
	
	// Number of frames emulated during last call to next_frame(). Indicates whether
	// movie playback is synchronized. 0) no emulation took place, 1) one frame emulated,
	// 2) two frames emulated
	int frames_emulated() const;
	
	void set_pixels( void*, long );
private:
	// Not available, even though they are in Nes_Emu
	blargg_err_t open_rom( Nes_Rom const* );
	nes_time_t emulate_frame( unsigned long joypad1, unsigned long joypad2 );
	Nes_Apu& get_apu();
	class Nes_Vrc6* get_vrc6();
	void set_timestamp( frame_count_t );
	frame_count_t timestamp() const;
	void close_rom();
// End of public interface  
protected:
	enum { fade_size_ = 384 };
	void fade_samples_( blip_sample_t* p, int size, int step );
	virtual void clear_cache();
	void seek_( frame_count_t );
	void play_frame_();
	int samples_per_frame() const { return sound_buf->samples_per_frame(); }
private:
	Multi_Buffer* sound_buf;
	Multi_Buffer* default_sound_buf;
	Multi_Buffer* nonlinearizer;
	nes_time_t sound_buf_end; // non-zero when skipping frames and buffer contains garbage
	Nes_Rom rom;
	BOOST::uint8_t* pixels;
	long row_bytes;
	int frames_emulated_;
	int channel_count_;
	equalizer_t equalizer_;
	blargg_err_t init();
	
	Nes_Film default_film;
	frame_count_t circular_duration;
	blargg_err_t reapply_circularity();
	bool fade_sound_in;
	bool fade_sound_out;
	bool skip_next_frame;
	
	Nes_Film* film;
	typedef Nes_Film::joypad_t joypad_t;
	joypad_t last_joypad;
	bool movie_resync_enabled;
	void emulate_frame_( joypad_t joypad, bool playing, bool rendering = false );
	void render_frame_( joypad_t joypad, bool playing );
	Nes_Snapshot const* nearest_snapshot( frame_count_t ) const;
	void load_snapshot_( Nes_Snapshot const& );
	
	// to do: make adjustable at init time so it can be made to match
	// Nes_Rewinder's frame count (once that also becomes adjustable)
	enum { cache_period_ = frames_per_second };
	Nes_Snapshot_Array cache;
	int cache_index( frame_count_t ) const;
};

inline const Nes_Recorder::equalizer_t& Nes_Recorder::equalizer() const { return equalizer_; }

inline int Nes_Recorder::channel_count() const { return channel_count_; }

inline int Nes_Recorder::frames_emulated() const { return frames_emulated_; }

inline Nes_Film& Nes_Recorder::get_film() const { return *film; }

inline bool Nes_Recorder::has_battery_ram() const { return rom.has_battery_ram(); }

inline long Nes_Recorder::samples_avail() const { return sound_buf->samples_avail(); }

inline void Nes_Recorder::set_pixels( void* p, long rb )
{
	pixels = (BOOST::uint8_t*) p;
	row_bytes = rb;
}

#endif

