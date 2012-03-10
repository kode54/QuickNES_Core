#include "stdafx.h"
#include "window_helper.h"

bool container_window::class_release()
{
	get_class_data().refcount--;
	if (!get_class_data().refcount && get_class_data().class_registered)
	{
		get_class_data().class_registered = !UnregisterClass(get_class_data().class_name, ATL::_AtlBaseModule.GetResourceInstance());
	}
	return get_class_data().class_registered == false;
}

container_window::container_window() : wnd_host(0){};

HWND container_window::create(HWND wnd_parent, LPVOID create_param)
{
	
	ensure_class_registered();
	LPVOID createparams[2] = {this, create_param};
	wnd_host = CreateWindowEx(get_class_data().ex_styles, get_class_data().class_name, get_class_data().class_name,
		get_class_data().styles, 0, 0, 0, 0,
		wnd_parent, 0, ATL::_AtlBaseModule.GetResourceInstance(), &createparams);
	
	return wnd_host;
}

bool container_window::ensure_class_registered()
{
	get_class_data().refcount++;
	if (!get_class_data().class_registered)
	{
		WNDCLASS  wc;
		memset(&wc,0,sizeof(WNDCLASS));
		//		wc.style          = CS_DBLCLKS; //causes issue where double clicking resets cursor icon
		wc.lpfnWndProc    = (WNDPROC)window_proc;
		wc.hInstance      = ATL::_AtlBaseModule.GetResourceInstance();
		wc.hCursor        = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
		wc.hbrBackground  = (HBRUSH)(COLOR_BTNFACE+1);
		wc.lpszClassName  = get_class_data().class_name;
		wc.style = get_class_data().class_styles;
		wc.cbWndExtra  = get_class_data().extra_wnd_bytes;
		
		get_class_data().class_registered = (RegisterClass(&wc) != 0);
	}
	return get_class_data().class_registered;
}

void container_window::destroy() //if destroying someother way, you should make sure you call class_release()  properly
{
	DestroyWindow(wnd_host);
	class_release();
}

LRESULT WINAPI container_window::window_proc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	container_window * p_this;
	
	if(msg == WM_NCCREATE)
	{
		LPVOID * create_params = reinterpret_cast<LPVOID *>(((CREATESTRUCT *)(lp))->lpCreateParams);
		p_this = reinterpret_cast<container_window *>(create_params[0]); //retrieve pointer to class
		SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)p_this);//store it for future use
		if (p_this) p_this->wnd_host = wnd;
		
	}
	else
		p_this = reinterpret_cast<container_window*>(GetWindowLongPtr(wnd,GWLP_USERDATA));//if isnt wm_create, retrieve pointer to class
	
	if (p_this && p_this->get_class_data().want_transparent_background)
	{
		if (msg == WM_ERASEBKGND)
		{
			HDC dc = (HDC)wp;
			
			if (dc)
			{
				HWND wnd_parent = GetParent(wnd);
				POINT pt = {0, 0}, pt_old = {0,0};
				MapWindowPoints(wnd, wnd_parent, &pt, 1);
				OffsetWindowOrgEx(dc, pt.x, pt.y, &pt_old);
				SendMessage(wnd_parent, WM_ERASEBKGND,wp, 0);
				SetWindowOrgEx(dc, pt_old.x, pt_old.y, 0);
			}
			return TRUE;
		}
		else if (msg==WM_WINDOWPOSCHANGING && p_this)
		{
			HWND meh_lazy = GetWindow(wnd, GW_CHILD);
			RedrawWindow(meh_lazy, 0, 0, RDW_ERASE);
		}
	}
	
	return p_this ? p_this->on_message(wnd, msg, wp, lp) : DefWindowProc(wnd, msg, wp, lp);
}

HWND container_window::get_wnd()const{return wnd_host;}

