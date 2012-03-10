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

	virtual int get_speed() const = 0;

	virtual void set_speed( int ) = 0;

	virtual void set_paused( bool ) = 0;

	virtual void reset() = 0;

	virtual void set_focus( bool is_focused ) = 0;

	virtual void refocus( void * hwnd ) = 0;
};

input * create_input();

#endif