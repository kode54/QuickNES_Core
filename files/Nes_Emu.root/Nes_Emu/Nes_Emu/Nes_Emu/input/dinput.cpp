#include "stdafx.h"

#include "dinput.h"

#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>

#include <string>

#include <assert.h>

#include "guid_container.h"

class dinput_i : public dinput
{
	guid_container         * guids;

	LPDIRECTINPUTDEVICE8     lpdKeyboard;

	struct joy
	{
		unsigned             serial;
		LPDIRECTINPUTDEVICE8 device;
		GUID                 GuidInstance;
#ifdef UNICODE
		std::wstring         name;
#else
		std::string          name;
#endif

		joy( unsigned serial, const GUID & GuidInstance, const TCHAR * name, LPDIRECTINPUTDEVICE8 device )
		{
			this->serial       = serial;
			this->device       = device;
			this->GuidInstance = GuidInstance;
			this->name         = name;
		}
	};

	std::vector< joy >       joysticks;

	bool                     focused;

	// only needed by enum callbacks
	LPDIRECTINPUT8           lpDI;

public:
	dinput_i()
	{
		lpdKeyboard = 0;
		focused = true;
	}

	virtual ~dinput_i()
	{
		std::vector< joy >::iterator it;
		for ( it = joysticks.begin(); it < joysticks.end(); ++it )
		{
			it->device->Release();
			guids->remove( it->GuidInstance );
		}

		if ( lpdKeyboard ) lpdKeyboard->Release();
	}

	virtual const char* open( void * di8, void * hwnd, guid_container * guids )
	{
		lpDI = ( LPDIRECTINPUT8 ) di8;
		HWND hWnd = ( HWND ) hwnd;

		this->guids         = guids;

		if ( lpDI->CreateDevice( GUID_SysKeyboard, &lpdKeyboard, 0 ) != DI_OK )
			return "Creating keyboard input";

		if ( lpdKeyboard->SetDataFormat( &c_dfDIKeyboard ) != DI_OK )
			return "Setting keyboard input format";

		if ( lpdKeyboard->SetCooperativeLevel( hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND ) != DI_OK )
			return "Setting keyboard cooperative level";

		DIPROPDWORD value;
		memset( &value, 0, sizeof( value ) );
		value.diph.dwSize = sizeof( value );
		value.diph.dwHeaderSize = sizeof( value.diph );
		//value.diph.dwObj = 0;
		value.diph.dwHow = DIPH_DEVICE;
		value.dwData = 128;
		if ( lpdKeyboard->SetProperty( DIPROP_BUFFERSIZE, &value.diph ) != DI_OK )
			return "Setting keyboard buffer size";

		if ( lpDI->EnumDevices( DI8DEVCLASS_GAMECTRL, g_enum_callback, this, DIEDFL_ATTACHEDONLY ) != DI_OK )
			return "Enumerating joysticks";

		return 0;
	}

	virtual std::vector< di_event > read()
	{
		std::vector< di_event > events;
		di_event e;

		HRESULT hr;

		if ( focused )
		{
			e.type = di_event::ev_key;

			for ( ;; )
			{
				DIDEVICEOBJECTDATA buffer[ 16 ];
				DWORD n_events = 16;

				hr = lpdKeyboard->GetDeviceData( sizeof( DIDEVICEOBJECTDATA ), buffer, &n_events, 0 );

				if ( hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED )
				{
					lpdKeyboard->Acquire();
					break;
				}

				if ( hr != DI_OK && hr != DI_BUFFEROVERFLOW ) break;
				if ( n_events == 0 ) break;

				for ( unsigned i = 0; i < n_events; ++i )
				{
					DIDEVICEOBJECTDATA & od = buffer[ i ];
					e.key.which = od.dwOfs;

					if ( od.dwData & 0x80 ) e.key.type = di_event::key_down;
					else e.key.type = di_event::key_up;

					events.push_back( e );
				}
			}
		}

		e.type = di_event::ev_joy;

		std::vector< joy >::iterator it;
		for ( it = joysticks.begin(); it < joysticks.end(); ++it )
		{
			e.joy.serial = it->serial;

			it->device->Poll();

			for ( ;; )
			{
				DIDEVICEOBJECTDATA buffer[ 16 ];
				DWORD n_events = 16;

				hr = it->device->GetDeviceData( sizeof( DIDEVICEOBJECTDATA ), buffer, &n_events, 0 );

				if ( hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED )
				{
					it->device->Acquire();
					break;
				}

				if ( hr != DI_OK && hr != DI_BUFFEROVERFLOW ) break;
				if ( n_events == 0 ) break;

				for ( unsigned i = 0; i < n_events; ++i )
				{
					DIDEVICEOBJECTDATA & od = buffer[ i ];
					if ( od.dwOfs >= DIJOFS_X && od.dwOfs <= DIJOFS_SLIDER( 1 ) )
					{
						e.joy.type = di_event::joy_axis;
						e.joy.which = ( od.dwOfs - DIJOFS_X ) / ( DIJOFS_Y - DIJOFS_X );

						if ( od.dwData < 0x8000 ) e.joy.axis = di_event::axis_negative;
						else if ( od.dwData > 0x8000 ) e.joy.axis = di_event::axis_positive;
						else e.joy.axis = di_event::axis_center;

						events.push_back( e );
					}
					else if ( od.dwOfs >= DIJOFS_POV( 0 ) && od.dwOfs <= DIJOFS_POV( 3 ) )
					{
						e.joy.type = di_event::joy_pov;
						e.joy.which = ( od.dwOfs - DIJOFS_POV( 0 ) ) / ( DIJOFS_POV( 1 ) - DIJOFS_POV( 0 ) );
						e.joy.pov_angle = od.dwData;
						if ( e.joy.pov_angle != ~0 ) e.joy.pov_angle /= DI_DEGREES;

						events.push_back( e );
					}
					else if ( od.dwOfs >= DIJOFS_BUTTON( 0 ) && od.dwOfs <= DIJOFS_BUTTON( 31 ) )
					{
						e.joy.type = di_event::joy_button;
						e.joy.which = ( od.dwOfs - DIJOFS_BUTTON( 0 ) ) / ( DIJOFS_BUTTON( 1 ) - DIJOFS_BUTTON( 0 ) );

						if ( od.dwData & 0x80 ) e.joy.button = di_event::button_down;
						else e.joy.button = di_event::button_up;

						events.push_back( e );
					}
				}
			}
		}

		return events;
	}

	virtual void set_focus( bool is_focused )
	{
		focused = is_focused;

		if ( ! is_focused && lpdKeyboard )
			lpdKeyboard->Unacquire();
	}

	virtual void refocus( void * hwnd )
	{
		if ( lpdKeyboard->SetCooperativeLevel( ( HWND ) hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND ) == DI_OK )
			set_focus( true );
	}

	virtual const TCHAR * get_joystick_name( unsigned n )
	{
		std::vector< joy >::iterator it;
		for ( it = joysticks.begin(); it < joysticks.end(); ++it )
		{
			if ( it->serial == n )
			{
				return it->name.c_str();
			}
		}
		return _T( "Unknown joystick" );
	}

private:
	static BOOL PASCAL g_enum_callback( LPCDIDEVICEINSTANCEW lpDev, LPVOID lpContext )
	{
		dinput_i * p_this = ( dinput_i * ) lpContext;
		return p_this->enum_callback( lpDev );
	}

	BOOL enum_callback( LPCDIDEVICEINSTANCE lpDev )
	{
		LPDIRECTINPUTDEVICE8 lpdJoy;

		if ( lpDI->CreateDevice( lpDev->guidInstance, &lpdJoy, 0 ) != DI_OK )
		{
			//OutputDebugString(_T("CreateDevice\n"));
			return DIENUM_CONTINUE;
		}

		if ( lpdJoy->SetDataFormat( &c_dfDIJoystick ) != DI_OK )
		{
			//OutputDebugString(_T("SetDataFormat\n"));
			lpdJoy->Release();
			return DIENUM_CONTINUE;
		}

		/*if ( lpdJoy->SetCooperativeLevel( hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND ) != DI_OK )
		{
			lpdJoy->Release();
			return DIENUM_CONTINUE;
		}*/

		lpdJoy->EnumObjects( g_enum_objects_callback, this, DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV );

		DIPROPDWORD value;
		memset( &value, 0, sizeof( value ) );
		value.diph.dwSize = sizeof( value );
		value.diph.dwHeaderSize = sizeof( value.diph );
		//value.diph.dwObj = 0;
		value.diph.dwHow = DIPH_DEVICE;
		value.dwData = 128;
		/*if (*/ lpdJoy->SetProperty( DIPROP_BUFFERSIZE, &value.diph ); /*!= DI_OK )
		{
			OutputDebugString(_T("SetProperty DIPROP_BUFFERSIZE\n"));
			lpdJoy->Release();
			return DIENUM_CONTINUE;
		}*/

		{
			DIPROPRANGE diprg; 
			diprg.diph.dwSize       = sizeof( diprg ); 
			diprg.diph.dwHeaderSize = sizeof( diprg.diph ); 
			diprg.diph.dwHow        = DIPH_BYOFFSET; 

			diprg.lMin = 0;
			diprg.lMax = 0xFFFF;

			DIPROPDWORD didz;
			didz.diph.dwSize        = sizeof( didz );
			didz.diph.dwHeaderSize  = sizeof( didz.diph );
			didz.diph.dwHow         = DIPH_BYOFFSET;

			didz.dwData = 0x2000;

			for ( unsigned i = DIJOFS_X; i <= DIJOFS_SLIDER( 1 ); i += ( DIJOFS_Y - DIJOFS_X ) )
			{
				diprg.diph.dwObj = i;
				didz.diph.dwObj  = i;
				lpdJoy->SetProperty( DIPROP_RANGE, & diprg.diph );
				lpdJoy->SetProperty( DIPROP_DEADZONE, & didz.diph );
			}
		}

		if ( lpdJoy->Acquire() != DI_OK )
		{
			//OutputDebugString(_T("Acquire\n"));
			lpdJoy->Release();
			return DIENUM_CONTINUE;
		}

		joysticks.push_back( joy( guids->add( lpDev->guidInstance ), lpDev->guidInstance, lpDev->tszInstanceName, lpdJoy ) );

		return DIENUM_CONTINUE;
	}

	static BOOL PASCAL g_enum_objects_callback( LPCDIDEVICEOBJECTINSTANCEW lpDOI, LPVOID lpContext )
	{
		return DIENUM_CONTINUE;
	}
};

dinput * create_dinput()
{
	return new dinput_i;
}