#ifndef _bind_list_h_
#define _bind_list_h_

#include "dinput.h"
#include "guid_container.h"

class Data_Reader;
class Data_Writer;

class bind_list
{
public:
	enum
	{
		bind_pad_0_a,
		bind_pad_0_b,
		bind_pad_0_select,
		bind_pad_0_start,
		bind_pad_0_up,
		bind_pad_0_down,
		bind_pad_0_left,
		bind_pad_0_right,

		bind_pad_1_a,
		bind_pad_1_b,
		bind_pad_1_select,
		bind_pad_1_start,
		bind_pad_1_up,
		bind_pad_1_down,
		bind_pad_1_left,
		bind_pad_1_right,

		bind_rewind_toggle,
		bind_rewind_hold,
		bind_rewind_analog,

		bind_forward_toggle,
		bind_forward_hold,
		bind_forward_analog,

		bind_joy_0_rapid,
		bind_joy_1_rapid,

		bind_pause_toggle,
		bind_pause_hold,

		bind_frame_advance,
		bind_frame_advance_hold
	};

	virtual ~bind_list() {}

	virtual bind_list * copy() = 0;

	// list manipulation
	virtual void add( const dinput::di_event &, unsigned action ) = 0;

	virtual unsigned get_count( ) = 0;

	virtual void get( unsigned index, dinput::di_event &, unsigned & action ) = 0;

	virtual void remove( unsigned index ) = 0;

	virtual void clear( ) = 0;

	// configuration file
	virtual const char * load( Data_Reader & ) = 0;

	virtual const char * save( Data_Writer & ) = 0;

	// input handling
	virtual void process( std::vector< dinput::di_event > & ) = 0;
	
	virtual unsigned read( unsigned ) = 0;

	virtual void strobe( unsigned ) = 0;

	virtual int get_speed() const = 0;

	virtual void set_speed( int ) = 0;

	virtual void set_paused( bool ) = 0;

	virtual void reset() = 0;

	virtual void update() = 0;
};

bind_list * create_bind_list( guid_container * );

#endif