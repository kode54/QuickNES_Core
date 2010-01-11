#ifndef _display_h_
#define _display_h_

#include <windows.h>

struct core_config_t;
struct sound_config_t;

class display
{
public:
	virtual ~display() {}

	virtual const char* open( unsigned buffer_width, unsigned buffer_height, HWND hWnd ) = 0;

	virtual void update_position() = 0;

	virtual const char* lock_framebuffer( void *& buffer, unsigned & pitch ) = 0;
	virtual const char* unlock_framebuffer( ) = 0;

	//virtual const char* update_palette( const unsigned char * source_pal, const unsigned char * source_list, unsigned color_first, unsigned color_count ) = 0;

	virtual const char* paint( /*RECT rcSource,*/ bool wait ) = 0;

	virtual void repaint() = 0;

	virtual void clear() = 0;
};

display * create_display();
display * create_display_d3d9();
display * create_display_opengl();

#endif