#include "stdafx.h"

#include "bind_list.h"

#include <assert.h>

#include <windows.h>

#include "nes_emu/abstract_file.h"

class bind_list_i : public bind_list
{
	guid_container * guids;

	unsigned char joy[2];

	int speed;

	bool paused;

	int frames_until_paused;

	bool rapid_enable[2];

	Joypad_Filter filter[2];

	struct bind
	{
		unsigned         action;
		dinput::di_event e;
	};

	std::vector< bind > list;

	/*CRITICAL_SECTION sync;

	inline void lock()
	{
		EnterCriticalSection( & sync );
	}

	inline void unlock()
	{
		LeaveCriticalSection( & sync );
	}*/

#define lock()
#define unlock()

	void press( unsigned which, unsigned value )
	{
		if ( which >= bind_pad_0_a && which <= bind_pad_0_right ) joy[ 0 ] |= 1 << which;

		else if ( which >= bind_pad_1_a && which <= bind_pad_1_right ) joy[ 1 ] |= 1 << ( which - bind_pad_1_a );

		else if ( which == bind_rewind_toggle )
		{
			if ( speed < 0 ) speed = 1;
			else speed = -1;
		}

		else if ( which == bind_rewind_hold ) speed = -1;

		else if ( which == bind_rewind_analog )
		{
			speed = -10 * ((int)value) / 65535;
			if ( speed > -1 ) speed = -1;
		}

		else if ( which == bind_forward_toggle )
		{
			if ( speed > 1 ) speed = 1;
			else speed = 10;
		}
		
		else if ( which == bind_forward_hold ) speed = 10;

		else if ( which == bind_forward_analog )
		{
			speed = 10 * ((int)value) / 65535;
			if ( speed < 1 ) speed = 1;
		}

		else if ( which == bind_joy_0_rapid ) rapid_enable[ 0 ] = ! rapid_enable[ 0 ];

		else if ( which == bind_joy_1_rapid ) rapid_enable[ 1 ] = ! rapid_enable[ 1 ];

		else if ( which == bind_pause_toggle ) paused = ! paused;
		else if ( which == bind_pause_hold ) paused = true;

		else if ( which == bind_frame_advance ) { paused = false; frames_until_paused = 2; }
	}

	void release( unsigned which, unsigned value )
	{
		if ( which >= bind_pad_0_a && which <= bind_pad_0_right ) joy[ 0 ] &= ~( 1 << which );

		else if ( which >= bind_pad_1_a && which <= bind_pad_1_right ) joy[ 1 ] &= ~ ( 1 << ( which - bind_pad_1_a ) );

		else if ( which == bind_rewind_hold ) speed = 1;

		else if ( which == bind_rewind_analog )
		{
			speed = -10 * ( ( int ) value ) / 65535;
			if ( speed == 0 ) speed = 1;
		}

		else if ( which == bind_forward_hold ) speed = 1;

		else if ( which == bind_forward_analog )
		{
			speed = 10 * ( ( int ) value ) / 65535;
			if ( speed < 1 ) speed = 1;
		}

		else if ( which == bind_pause_hold ) paused = false;
	}

public:
	bind_list_i( guid_container * _guids ) : guids( _guids )
	{
		reset();
		//InitializeCriticalSection( & sync );
	}

	virtual ~bind_list_i()
	{
		clear();
		//DeleteCriticalSection( & sync );
	}

	virtual bind_list * copy()
	{
		lock();
			bind_list * out = new bind_list_i( guids );

			std::vector< bind >::iterator it;
			for ( it = list.begin(); it < list.end(); ++it )
			{
				out->add( it->e, it->action );
			}
		unlock();

		return out;
	}

	virtual void add( const dinput::di_event & e, unsigned action )
	{
		lock();
			if ( e.type == dinput::di_event::ev_joy )
			{
				GUID guid;
				if ( guids->get_guid( e.joy.serial, guid ) )
					guids->add( guid );
			}

			bind b;
			b.e = e;
			b.action = action;

			list.push_back( b );
		unlock();
	}

	virtual unsigned get_count()
	{
		lock();
			unsigned count = list.end() - list.begin();
		unlock();

		return count;
	}

	virtual void get( unsigned index, dinput::di_event & e, unsigned & action )
	{
		lock();
			assert( index < get_count() );

			const bind & b = list[ index ];

			e = b.e;
			action = b.action;
		unlock();
	}

	virtual void remove( unsigned index )
	{
		lock();
			assert( index < get_count() );

			std::vector< bind >::iterator it = list.begin() + index;
			GUID guid;

			if ( it->e.type == dinput::di_event::ev_joy )
			{
				if ( guids->get_guid( it->e.joy.serial, guid ) )
					guids->remove( guid );
			}

			list.erase( it );
		unlock();
	}

	virtual void clear()
	{
		lock();
			std::vector< bind >::iterator it;
			GUID guid;
			for ( it = list.begin(); it < list.end(); ++it )
			{
				if ( it->e.type == dinput::di_event::ev_joy )
				{
					if ( guids->get_guid( it->e.joy.serial, guid ) )
						guids->remove( guid );
				}
			}
			list.clear();
		unlock();
	}

	virtual const char * load( Data_Reader & in )
	{
		const char * err = "Invalid input config file";

		lock();
			clear();
			reset();

			do
			{
				unsigned n;
				err = in.read( & n, sizeof( n ) ); if ( err ) break;

				for ( unsigned i = 0; i < n; ++i )
				{
					bind b;

					err = in.read( & b.action, sizeof( b.action ) ); if ( err ) break;
					err = in.read( & b.e.type, sizeof( b.e.type ) ); if ( err ) break;

					// Action remaps
					if ( b.action >= 16 && b.action <= 23 )
					{
						static const unsigned action_remap[] = { 0, 1, 3, 4, 6, 7, 2, 5 };
						b.action = action_remap[ b.action - 16 ] + 16;
					}

					if ( b.e.type == dinput::di_event::ev_key )
					{
						err = in.read( & b.e.key.which, sizeof( b.e.key.which ) ); if ( err ) break;
						list.push_back( b );
					}
					else if ( b.e.type == dinput::di_event::ev_joy )
					{
						GUID guid;
						err = in.read( & guid, sizeof( guid ) ); if ( err ) break;
						err = in.read( & b.e.joy.type, sizeof( b.e.joy.type ) ); if ( err ) break;
						err = in.read( & b.e.joy.which, sizeof( b.e.joy.which ) ); if ( err ) break;
						if ( b.e.joy.type == dinput::di_event::joy_axis )
						{
							err = in.read( & b.e.joy.axis, sizeof( b.e.joy.axis ) ); if ( err ) break;
						}
						else if ( b.e.joy.type == dinput::di_event::joy_button )
						{
						}
						else if ( b.e.joy.type == dinput::di_event::joy_pov )
						{
							err = in.read( & b.e.joy.pov_angle, sizeof( b.e.joy.pov_angle ) ); if ( err ) break;
						}
						else break;
						b.e.joy.serial = guids->add( guid );
						list.push_back( b );
					}
					else if ( b.e.type == dinput::di_event::ev_xinput )
					{
						err = in.read( & b.e.xinput.index, sizeof( b.e.xinput.index ) ); if ( err ) break;
						err = in.read( & b.e.xinput.type, sizeof( b.e.xinput.type ) ); if ( err ) break;
						err = in.read( & b.e.xinput.which, sizeof( b.e.xinput.which ) ); if ( err ) break;
						if ( b.e.xinput.type == dinput::di_event::xinput_axis )
						{
							err = in.read( & b.e.xinput.axis, sizeof( b.e.xinput.axis ) ); if ( err ) break;
						}
						list.push_back( b );
					}
				}

				err = 0;
			}
			while ( 0 );

		unlock();

		return err;
	}

	virtual const char * save( Data_Writer & out )
	{
		const char * err;

		lock();
			do
			{
				unsigned n = get_count();
				err = out.write( & n, sizeof( n ) ); if ( err ) break;

				std::vector< bind >::iterator it;
				for ( it = list.begin(); it < list.end(); ++it )
				{
					// Action remaps
					if ( it->action >= 16 && it->action <= 23 )
					{
						static const unsigned action_remap[] = { 0, 1, 6, 2, 3, 7, 4, 5 };
						it->action = action_remap[ it->action - 16 ] + 16;
					}

					err = out.write( & it->action, sizeof( it->action ) ); if ( err ) break;
					err = out.write( & it->e.type, sizeof( it->e.type ) ); if ( err ) break;

					if ( it->e.type == dinput::di_event::ev_key )
					{
						err = out.write( & it->e.key.which, sizeof( it->e.key.which ) ); if ( err ) break;
					}
					else if ( it->e.type == dinput::di_event::ev_joy )
					{
						GUID guid;
						if ( ! guids->get_guid( it->e.joy.serial, guid ) ) { err = "GUID missing"; break; }
						err = out.write( & guid, sizeof( guid ) ); if ( err ) break;
						err = out.write( & it->e.joy.type, sizeof( it->e.joy.type ) ); if ( err ) break;
						err = out.write( & it->e.joy.which, sizeof( it->e.joy.which ) ); if ( err ) break;
						if ( it->e.joy.type == dinput::di_event::joy_axis )
						{
							err = out.write( & it->e.joy.axis, sizeof( it->e.joy.axis ) ); if ( err ) break;
						}
						else if ( it->e.joy.type == dinput::di_event::joy_pov )
						{
							err = out.write( & it->e.joy.pov_angle, sizeof( it->e.joy.pov_angle ) ); if ( err ) break;
						}
					}
					else if ( it->e.type == dinput::di_event::ev_xinput )
					{
						err = out.write( & it->e.xinput.index, sizeof( it->e.xinput.index ) ); if ( err ) break;
						err = out.write( & it->e.xinput.type, sizeof( it->e.xinput.type ) ); if ( err ) break;
						err = out.write( & it->e.xinput.which, sizeof( it->e.xinput.which ) ); if ( err ) break;
						if ( it->e.xinput.type == dinput::di_event::xinput_axis )
						{
							err = out.write( & it->e.xinput.axis, sizeof( it->e.xinput.axis ) ); if ( err ) break;
						}
					}
				}
			}
			while ( 0 );

		unlock();

		return err;
	}

	virtual void process( std::vector< dinput::di_event > & events )
	{
		lock();
			std::vector< dinput::di_event >::iterator it;
			for ( it = events.begin(); it < events.end(); ++it )
			{
				std::vector< bind >::iterator itb;
				if ( it->type == dinput::di_event::ev_key ||
					( it->type == dinput::di_event::ev_joy &&
					  it->joy.type == dinput::di_event::joy_button ) ||
					( it->type == dinput::di_event::ev_xinput &&
					  it->xinput.type == dinput::di_event::xinput_button ) )
				{
					// two state
					if ( it->type == dinput::di_event::ev_key )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_key &&
								itb->e.key.which == it->key.which )
							{
								if ( it->key.type == dinput::di_event::key_down ) press( itb->action, 65535 );
								else release( itb->action, 0 );
							}
						}
					}
					else
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_button &&
								itb->e.joy.which == it->joy.which ) ||
								( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_button &&
								itb->e.xinput.which == it->xinput.which ) )
							{
								if ( ( it->type == dinput::di_event::ev_joy && it->joy.button == dinput::di_event::button_down ) ||
									( it->type == dinput::di_event::ev_xinput && it->xinput.button == dinput::di_event::button_down ) ) press( itb->action, 65535 );
								else release( itb->action, 0 );
							}
						}
					}
				}
				else if ( it->type == dinput::di_event::ev_joy )
				{
					// mutually exclusive in class set
					if ( it->joy.type == dinput::di_event::joy_axis )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_axis &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.axis != itb->e.joy.axis ) release( itb->action, ( it->joy.axis == dinput::di_event::axis_negative ) ? ( ( 32767 - it->joy.value ) * 2 ) : ( ( it->joy.value - 32768 ) * 2 ) );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_axis &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.axis == itb->e.joy.axis ) press( itb->action, ( it->joy.axis == dinput::di_event::axis_negative ) ? ( ( 32767 - it->joy.value ) * 2 ) : ( ( it->joy.value - 32768 ) * 2 ) );
							}
						}
					}
					else if ( it->joy.type == dinput::di_event::joy_pov )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_pov &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.pov_angle != itb->e.joy.pov_angle ) release( itb->action, 0 );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_pov &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.pov_angle == itb->e.joy.pov_angle ) press( itb->action, 65535 );
							}
						}
					}
				}
				else if ( it->type == dinput::di_event::ev_xinput )
				{
					// mutually exclusive in class set
					if ( it->xinput.type == dinput::di_event::xinput_axis )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_axis &&
								itb->e.xinput.which == it->xinput.which )
							{
								if ( it->xinput.axis != itb->e.xinput.axis ) release( itb->action, ( it->joy.axis == dinput::di_event::axis_negative ) ? ( ( 32767 - it->joy.value ) * 2 ) : ( ( it->joy.value - 32768 ) * 2 ) );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_axis &&
								itb->e.xinput.which == it->xinput.which )
							{
								if ( it->xinput.axis == itb->e.xinput.axis ) press( itb->action, ( it->xinput.axis == dinput::di_event::axis_negative ) ? ( ( 32767 - it->joy.value ) * 2 ) : ( ( it->joy.value - 32768 ) * 2 ) );
							}
						}
					}
					else if ( it->xinput.type == dinput::di_event::xinput_trigger )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_xinput &&
								itb->e.xinput.index == it->xinput.index &&
								itb->e.xinput.type == dinput::di_event::xinput_trigger &&
								itb->e.xinput.which == it->xinput.which )
							{
								if ( it->xinput.button == dinput::di_event::button_down ) press( itb->action, it->xinput.value * 257 );
								else release( itb->action, it->xinput.value * 257 );
							}
						}
					}
				}
			}
		unlock();
	}

	virtual unsigned read( unsigned joy )
	{
		assert( joy <= 1 );
		register unsigned in = ( this->joy[ joy ] & ~3 ) | ( ( this->joy[ joy ] & 3 ) << ( rapid_enable[ joy ] * 8 ) );
		return filter[ joy ].process( in );
	}

	virtual void strobe( unsigned joy )
	{
		assert( joy <= 1 );
		filter[ joy ].clock_turbo();
	}

	virtual void update()
	{
		if ( frames_until_paused && !--frames_until_paused ) paused = true;
	}

	virtual int get_speed() const
	{
		return paused ? 0 : speed;
	}

	virtual void set_speed( int speed )
	{
		this->speed = speed;
	}

	virtual void set_paused( bool paused )
	{
		this->paused = paused;
	}

	virtual void reset()
	{
		joy[ 0 ] = 0;
		joy[ 1 ] = 0;

		speed = 1;

		paused = false;

		frames_until_paused = 0;

		rapid_enable[ 0 ] = false;
		rapid_enable[ 1 ] = false;
		
		/*rapid_toggle[ 0 ] = false;
		rapid_toggle[ 1 ] = false;*/
	}
};

bind_list * create_bind_list( guid_container * guids )
{
	return new bind_list_i( guids );
}
