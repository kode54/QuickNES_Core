#ifndef _input_h_
#define _input_h_

#include <windows.h>

class Data_Reader;
class Data_Writer;

class input
{
public:
	virtual ~input() {}

	virtual const char* open( HINSTANCE hInstance, HWND hWnd ) = 0;

	// configuration
	virtual const char* load( Data_Reader & ) = 0;

	virtual const char* save( Data_Writer & ) = 0;

	virtual void configure( void * hinstance, void * hwnd ) = 0;

	// input
	virtual void poll() = 0;

	virtual unsigned read() = 0;

	virtual void strobe() = 0;

	virtual int get_direction() = 0;

	virtual void set_direction( int ) = 0;

	virtual bool get_forward() = 0;

	virtual void set_forward( bool ) = 0;

	virtual void reset() = 0;

	virtual void set_focus( bool is_focused ) = 0;

	virtual void refocus( void * hwnd ) = 0;
};

input * create_input();

#endif