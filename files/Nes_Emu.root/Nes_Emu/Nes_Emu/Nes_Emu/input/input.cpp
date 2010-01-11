#include "stdafx.h"

#include "input.h"

#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>

#include "guid_container.h"
#include "dinput.h"
#include "bind_list.h"
#include "input_config.h"

#include "nes_emu/abstract_file.h"

#pragma comment( lib, "dinput8.lib" )
#pragma comment( lib, "dxguid.lib" )

static const GUID g_signature = { 0x925c561e, 0xfdfe, 0x40b3, { 0x9a, 0xe9, 0xbf, 0x82, 0x85, 0x86, 0x4b, 0xb5 } };

class input_i_dinput : public input
{
	unsigned                bits;

	LPDIRECTINPUT8          lpDI;

	guid_container        * guids;

	dinput                * di;

	bind_list             * bl;

	// needed by SetCooperativeLevel inside enum callback
	//HWND                    hWnd;

public:
	input_i_dinput()
	{
		bits = 0;

		lpDI = 0;

		guids = 0;

		di = 0;

		bl = 0;
	}

	virtual ~input_i_dinput()
	{
		if ( bl )
		{
			delete bl;
			bl = 0;
		}

		if ( di )
		{
			delete di;
			di = 0;
		}

		if ( guids )
		{
			delete guids;
			guids = 0;
		}

		if ( lpDI )
		{
			lpDI->Release();
			lpDI = 0;
		}
	}

	virtual const char* open( HINSTANCE hInstance, HWND hWnd )
	{
		if ( DirectInput8Create( hInstance, DIRECTINPUT_VERSION,
#ifdef UNICODE
			IID_IDirectInput8W
#else
			IID_IDirectInput8A
#endif
			, ( void ** ) &lpDI, 0 ) != DI_OK )
			return "Creating DirectInput object";

		guids = create_guid_container();

		di = create_dinput();
		
		const char * err = di->open( lpDI, hWnd, guids );
		if ( err ) return err;

		bl = create_bind_list( guids );

		return 0;
	}

	virtual const char* load( Data_Reader & in )
	{
		const char * err;
		GUID check;

		err = in.read( & check, sizeof( check ) );
		if ( ! err ) { if ( check != g_signature ) err = "Invalid signature"; }
		if ( ! err ) err = bl->load( in );

		return err;
	}

	virtual const char* save( Data_Writer & out )
	{
		const char * err;
		
		err = out.write( & g_signature, sizeof( g_signature ) );
		if ( ! err ) err = bl->save( out );

		return err;
	}

	virtual void configure( void * hinstance, void * hwnd )
	{
		bind_list * bl = this->bl->copy();

		if ( do_input_config( lpDI, hinstance, hwnd, guids, bl ) )
		{
			delete this->bl;
			this->bl = bl;
		}
		else delete bl;
	}

	virtual void poll()
	{
		bl->process( di->read() );
	}

	virtual unsigned read()
	{
		unsigned ret;

		ret = bl->read( 0 );
		ret |= bl->read( 1 ) << 8;

		return ret;
	}

	virtual void strobe()
	{
		bl->strobe( 0 );
		bl->strobe( 1 );
	}

	virtual int get_direction()
	{
		int ret;

		ret = bl->get_direction();

		return ret;
	}

	virtual void reset()
	{
		bl->reset();
	}

	virtual void set_direction( int dir )
	{
		bl->set_direction( dir );
	}

	virtual void set_focus( bool is_focused )
	{
		di->set_focus( is_focused );
	}

	virtual void refocus( void * hwnd )
	{
		di->refocus( hwnd );
	}
};

input * create_input()
{
	return new input_i_dinput;
}