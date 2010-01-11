#ifndef _sound_out_h_
#define _sound_out_h_

class sound_out
{
public:
	virtual ~sound_out() {}

	virtual const char* open( unsigned sample_rate, unsigned nch, unsigned max_samples_per_frame, unsigned num_frames ) = 0;

	virtual const char* write_frame( void * buffer, unsigned num_samples, bool wait = true ) = 0;

	virtual const char* pause( bool pausing ) = 0;

	virtual unsigned buffered() = 0;
};

sound_out * create_sound_out();

#endif