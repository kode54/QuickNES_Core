#ifndef _MUSICMUSIC_WINDOW_H_
#define _MUSICMUSIC_WINDOW_H_

/**
 * \file window_helper.h
 * Simple helper that creates a host window for you to catch notifications from e.g. common controls
 * \author musicmusic
 */

#define __implement_get_class_data(class_name,want_transparent_background) \
	static container_window::class_data my_class_data= {class_name, 0, false, want_transparent_background, 0, WS_CHILD|WS_CLIPCHILDREN, WS_EX_CONTROLPARENT, 0};	\
		return my_class_data

#define __implement_get_class_data_ex(class_name,want_transparent_background,extra_wnd_bytes,styles,stylesex,classstyles) \
	static container_window::class_data my_class_data= {class_name, 0, false, want_transparent_background,extra_wnd_bytes,styles,stylesex, classstyles};	\
		return my_class_data

class container_window
{
protected:
	HWND wnd_host;
	
public:
	struct class_data
	{
		const TCHAR * class_name;
		long refcount;
		bool class_registered;
		bool want_transparent_background;
		int extra_wnd_bytes;
		long styles;
		long ex_styles;
		long class_styles;
	};

	/**
	* Gets window class data.
	*
	* \return					Reference to class_data
	* 
	* \par Sample implementation:
	* \code
	* virtual class_data & get_class_data() const 
	* {
	* __implement_get_class_data(
	* "My Window Class", //window class name
	* true); //want transparent background (i.e. for toolbar controls)
	* }
	* \endcode
	* \see __implement_get_class_data, __implement_get_class_data_ex
	*/
	virtual class_data & get_class_data() const = 0;

	container_window();
	
	HWND create(HWND wnd_parent, LPVOID create_param = 0);
	
	bool ensure_class_registered();	
	bool class_release();

	void destroy(); //if destroying someother way, you should make sure you call class_release()  properly
	
	static LRESULT WINAPI window_proc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
	
	HWND get_wnd()const;
	
	//override me
	//you won't get called for WM_ERASEBKGRND if you specify want_transparent_background
	virtual LRESULT WINAPI on_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)=0;
};

#endif
