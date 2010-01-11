#include "stdafx.h"

#include "bind_list.h"

#include <assert.h>

#include <windows.h>

#include "nes_emu/abstract_file.h"

class Joypad_Processor {
public:
    Joypad_Processor() : prev( 0 ), mask( ~0x50 ) { }
 
    void clock_turbo() { mask ^= 0x300; }
 
    // bits 8 and 9 generate turbo into bits 0 and 1
    int process( int joypad );
 
private:
    int prev;
    int mask;
};
 
int Joypad_Processor::process( int joypad )
{
    int changed = prev ^ joypad;
    prev = joypad;
 
    // reset turbo if button just pressed, to avoid delaying button press
    mask |= changed & 0x300 & joypad;
 
    // prevent left+right and up+down (prefer most recent one pressed)
    if ( changed & 0xf0 )
    {
        int diff = joypad & ~mask;
 
        if ( diff & 0x30 )
            mask ^= 0x30;
 
        if ( diff & 0xc0 )
            mask ^= 0xc0;
    }
 
    // mask and combine turbo bits
    joypad &= mask;
    return (joypad >> 8 & 3) | joypad;
}

class bind_list_i : public bind_list
{
	guid_container * guids;

	unsigned char joy[2];

	int direction;

	bool rapid_enable[2];

	Joypad_Processor processor[2];

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

	void press( unsigned which )
	{
		if ( which >= bind_pad_0_a && which <= bind_pad_0_right ) joy[ 0 ] |= 1 << which;

		else if ( which >= bind_pad_1_a && which <= bind_pad_1_right ) joy[ 1 ] |= 1 << ( which - bind_pad_1_a );

		else if ( which == bind_rewind_toggle ) direction = - direction;

		else if ( which == bind_rewind_hold ) direction = -1;

		else if ( which == bind_joy_0_rapid ) rapid_enable[ 0 ] = ! rapid_enable[ 0 ];

		else if ( which == bind_joy_1_rapid ) rapid_enable[ 1 ] = ! rapid_enable[ 1 ];
	}

	void release( unsigned which )
	{
		if ( which >= bind_pad_0_a && which <= bind_pad_0_right ) joy[ 0 ] &= ~( 1 << which );

		else if ( which >= bind_pad_1_a && which <= bind_pad_1_right ) joy[ 1 ] &= ~ ( 1 << ( which - bind_pad_1_a ) );

		else if ( which == bind_rewind_hold ) direction = 1;
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
					  it->joy.type == dinput::di_event::joy_button ) )
				{
					// two state
					if ( it->type == dinput::di_event::ev_key )
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_key &&
								itb->e.key.which == it->key.which )
							{
								if ( it->key.type == dinput::di_event::key_down ) press( itb->action );
								else release( itb->action );
							}
						}
					}
					else
					{
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_button &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.button == dinput::di_event::button_down ) press( itb->action );
								else release( itb->action );
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
								if ( it->joy.axis != itb->e.joy.axis ) release( itb->action );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_axis &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.axis == itb->e.joy.axis ) press( itb->action );
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
								if ( it->joy.pov_angle != itb->e.joy.pov_angle ) release( itb->action );
							}
						}
						for ( itb = list.begin(); itb < list.end(); ++itb )
						{
							if ( itb->e.type == dinput::di_event::ev_joy &&
								itb->e.joy.serial == it->joy.serial &&
								itb->e.joy.type == dinput::di_event::joy_pov &&
								itb->e.joy.which == it->joy.which )
							{
								if ( it->joy.pov_angle == itb->e.joy.pov_angle ) press( itb->action );
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
		return processor[ joy ].process( in );
	}

	virtual void strobe( unsigned joy )
	{
		assert( joy <= 1 );
		processor[ joy ].clock_turbo();
	}

	virtual int get_direction()
	{
		return direction;
	}

	virtual void set_direction( int dir )
	{
		direction = dir;
	}

	virtual void reset()
	{
		joy[ 0 ] = 0;
		joy[ 1 ] = 0;

		direction = 1;

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
