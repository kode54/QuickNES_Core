#ifndef _dinput_h_
#define _dinput_h_

#include <vector>

#include <tchar.h>

class guid_container;

class dinput
{
public:
	struct di_event
	{
		typedef enum
		{
			ev_key,
			ev_joy,
			ev_xinput
		} event_type;

		typedef enum
		{
			key_up,
			key_down
		} key_type;

		typedef enum
		{
			joy_axis,
			joy_button,
			joy_pov
		} joy_type;

		typedef enum
		{
			xinput_axis,
			xinput_trigger,
			xinput_button
		} xinput_type;

		typedef enum
		{
			axis_center,
			axis_negative,
			axis_positive
		} axis_motion;

		typedef enum
		{
			button_up,
			button_down
		} button_motion;

		event_type type;

		union
		{
			struct
			{
				key_type type;

				unsigned which;
			} key;

			struct
			{
				unsigned serial;

				joy_type type;

				unsigned which;

				union
				{
					axis_motion axis;

					button_motion button;

					unsigned pov_angle;
				};

				unsigned value;
			} joy;

			struct
			{
				unsigned index;

				xinput_type type;

				unsigned which;

				union
				{
					axis_motion axis;

					button_motion button;
				};

				unsigned value;
			} xinput;
		};
	};

	virtual ~dinput() {}

	virtual const char* open( void * di8, void * hwnd, guid_container * ) = 0;

	virtual std::vector< di_event > read() = 0;

	virtual void set_focus( bool ) = 0;

	virtual void refocus( void * hwnd ) = 0;

	virtual const TCHAR * get_joystick_name( unsigned ) = 0;
};

dinput * create_dinput();

#endif