#include "stdafx.h"

#include "display_config.h"

#include <windows.h>
#include <tchar.h>

#include <commctrl.h>

#include "../config/config.h"

#include "../resource.h"

class display_config
{
	display_config_t * config;

public:
	bool run( void * hinstance, void * hwnd, display_config_t * config )
	{
		this->config = config;
		
		return !! DialogBoxParam( ( HINSTANCE ) hinstance, MAKEINTRESOURCE( IDD_DISPLAY_CONFIG ), ( HWND ) hwnd, g_dlg_proc, ( LPARAM ) this );
	}

private:
	static INT_PTR CALLBACK g_dlg_proc( HWND w, UINT msg, WPARAM wp, LPARAM lp )
	{
		display_config * p_this;
	
		if(	msg == WM_INITDIALOG )
		{
			p_this = ( display_config * ) lp;
			SetWindowLongPtr( w, DWLP_USER, ( LONG_PTR ) p_this );
		}
		else
			p_this = reinterpret_cast< display_config * >( GetWindowLongPtr( w, DWLP_USER ) );
	
		return p_this ? p_this->DlgProc( w, msg, wp, lp ) : FALSE;
	}

	INT_PTR DlgProc( HWND w, UINT msg, WPARAM wp, LPARAM lp )
	{
		switch ( msg )
		{
		case WM_INITDIALOG:
			{
				SendDlgItemMessage( w, IDC_VSYNC, BM_SETCHECK, config->vsync, 0 );
			}
			break;

		case WM_COMMAND:
			switch ( wp )
			{
			case IDOK:
				config->vsync = SendDlgItemMessage( w, IDC_VSYNC, BM_GETCHECK, 0, 0 );
				EndDialog( w, 1 );
				break;

			case IDCANCEL:
				EndDialog( w, 0 );
				break;
			}
			break;
		}

		return FALSE;
	}
};

bool do_display_config( void * hinstance, void * hwnd, display_config_t * config )
{
	display_config sc;

	return sc.run( hinstance, hwnd, config );
}