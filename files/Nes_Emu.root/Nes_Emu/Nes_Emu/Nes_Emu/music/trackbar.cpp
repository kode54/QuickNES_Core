#include "stdafx.h"
#include "trackbar.h"
#include "uxtheme.h"

#include <commctrl.h>

#ifndef SelectPen
#define     SelectPen(hdc, hpen)    ((HPEN)SelectObject((hdc), (HGDIOBJ)(HPEN)(hpen)))
#endif

/**
 * \file trackbar.cpp
 * trackbar custom control
 * Copyright (C) 2005
 * \author musicmusic
 */

#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN                  0x020B
#endif

static HWND WINAPI _GetParent_test(HWND wnd);
static HWND WINAPI _GetParent_ga(HWND wnd);

static HWND ( WINAPI * _GetParent ) ( HWND ) = & _GetParent_test;
static HWND ( WINAPI * _GetAncestor ) ( HWND, UINT ) = 0;

static HWND WINAPI _GetParent_test(HWND wnd)
{
	HMODULE hUser = GetModuleHandle(_T("user32"));
	if ( hUser )
	{
		_GetAncestor = ( HWND ( WINAPI * ) ( HWND, UINT ) ) GetProcAddress(hUser, "GetAncestor");
		if ( _GetAncestor )
		{
			_GetParent = & _GetParent_ga;
			return _GetParent_ga( wnd );
		}
	}
	_GetParent = & GetParent;
	return GetParent( wnd );
}

#ifndef GA_PARENT
#define GA_PARENT 1
#endif

static HWND WINAPI _GetParent_ga(HWND wnd)
{
	return _GetAncestor(wnd, GA_PARENT);
}

HWND FindParentPopup(HWND wnd_child)
{
	HWND wnd_temp = _GetParent(wnd_child);
	
	while (wnd_temp && (GetWindowLong(wnd_temp, GWL_EXSTYLE) & WS_EX_CONTROLPARENT))
	{
		if (GetWindowLong(wnd_temp, GWL_STYLE) & WS_POPUP) break;
		else wnd_temp = _GetParent(wnd_temp);
	}
	return wnd_temp;
}

namespace musicmusic
{
	track_bar::~track_bar()
	{
		if ( m_uxtheme ) delete m_uxtheme;
	}

	track_bar::class_data & track_bar::get_class_data()const 
	{
		__implement_get_class_data(_T("musicmusic_track_bar"), false);
	}

	BOOL track_bar::create_tooltip(const TCHAR * text, POINT pt)
	{
		destroy_tooltip();

		m_wnd_tooltip = CreateWindowEx(WS_EX_TOPMOST|WS_EX_TRANSPARENT, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_NOPREFIX ,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, get_wnd(), 0, ATL::_AtlBaseModule.GetResourceInstance(), NULL);
		
		SetWindowPos(m_wnd_tooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		
		TOOLINFO ti;

		memset(&ti,0,sizeof(ti));
		
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_SUBCLASS|TTF_TRANSPARENT|TTF_TRACK|TTF_ABSOLUTE;
		ti.hwnd = wnd_host;
		ti.hinst = ATL::_AtlBaseModule.GetResourceInstance();
		ti.lpszText = const_cast<TCHAR *>(text);
		
		SendMessage(m_wnd_tooltip, TTM_ADDTOOL, 0, (LPARAM)  &ti);
		
		SendMessage(m_wnd_tooltip, TTM_TRACKPOSITION, 0, MAKELONG(pt.x+14, pt.y+21));
		SendMessage(m_wnd_tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)  &ti);	
		
		return TRUE;
	}

	void track_bar::destroy_tooltip()
	{
		if (m_wnd_tooltip) {DestroyWindow(m_wnd_tooltip); m_wnd_tooltip=0;}
	}


	BOOL track_bar::update_tooltip(POINT pt, const TCHAR * text)
	{
		if (!m_wnd_tooltip) return FALSE;

		SendMessage(m_wnd_tooltip, TTM_TRACKPOSITION, 0, MAKELONG(pt.x+14, pt.y+21));

		TOOLINFO ti;
		memset(&ti,0,sizeof(ti));
		
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_SUBCLASS|TTF_TRANSPARENT|TTF_TRACK|TTF_ABSOLUTE;
		ti.hwnd = get_wnd();
		ti.hinst = ATL::_AtlBaseModule.GetResourceInstance();
		ti.lpszText = const_cast<TCHAR *>(text);

		SendMessage(m_wnd_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)  &ti);

		return TRUE;
	}

	void track_bar::set_callback(track_bar_host * p_host)
	{
		m_host = p_host;
	}

	void track_bar::set_show_tooltips(bool val)
	{
		m_show_tooltips = val;
		if (!val)
			destroy_tooltip();
	}

	void track_bar::set_position_internal(unsigned pos)
	{
		if (!m_dragging)
		{
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(get_wnd(), &pt);
			update_hot_status(pt);
		}

		RECT rc;
		get_thumb_rect(&rc);
		HRGN rgn_old = CreateRectRgnIndirect(&rc);
		get_thumb_rect(pos, m_range_min, m_range_max, &rc);
		HRGN rgn_new = CreateRectRgnIndirect(&rc);
		CombineRgn(rgn_new, rgn_old, rgn_new, RGN_OR);
		DeleteObject(rgn_old);
		m_display_position = pos;
		//InvalidateRgn(m_wnd, rgn_new, TRUE);
		RedrawWindow(get_wnd(), 0, rgn_new, RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW|RDW_ERASENOW);
		DeleteObject(rgn_new);
	}

	void track_bar::set_position(unsigned pos)
	{
		m_position = pos;
		if (!m_dragging)
		{
			set_position_internal(pos);
		}
	}
	unsigned track_bar::get_position() const
	{
		return m_position;
	}

	void track_bar::set_range(unsigned min, unsigned max)
	{
		RECT rc;
		get_thumb_rect(&rc);
		HRGN rgn_old = CreateRectRgnIndirect(&rc);
		get_thumb_rect(m_display_position, min, max, &rc);
		HRGN rgn_new = CreateRectRgnIndirect(&rc);
		CombineRgn(rgn_new, rgn_old, rgn_new, RGN_OR);
		DeleteObject(rgn_old);
		m_range_min = min;
		m_range_max = max;
		RedrawWindow(get_wnd(), 0, rgn_new, RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW|RDW_ERASENOW);
		DeleteObject(rgn_new);
	}
	void track_bar::get_range(unsigned & min, unsigned & max) const
	{
		min = m_range_min;
		max = m_range_max;
	}

	void track_bar::set_enabled(bool enabled)
	{
		EnableWindow(get_wnd(), enabled);
	}
	bool track_bar::get_enabled() const
	{
		return IsWindowEnabled(get_wnd()) !=0;
	}

	void track_bar_impl::get_thumb_rect(unsigned pos, unsigned range_min, unsigned range_max, RECT * rc) const
	{
		RECT rc_client;
		GetClientRect(get_wnd(), &rc_client);
		unsigned cx = MulDiv(rc_client.bottom,9,20);
		rc->top = 2;
		rc->bottom = rc_client.bottom - 2;
		rc->left = ( range_max - range_min ) ? MulDiv(pos-range_min, rc_client.right-cx, range_max - range_min) : 0;
		rc->right = rc->left + cx;
	}

	void track_bar_impl::get_channel_rect(RECT * rc) const
	{
		RECT rc_client;
		GetClientRect(get_wnd(), &rc_client);
		unsigned cx = MulDiv(rc_client.bottom,9,20);
		rc->left = rc_client.left + cx/2;
		rc->right = rc_client.right - cx + cx/2;
		rc->top = rc_client.bottom/2-2;
		rc->bottom = rc_client.bottom/2+2;
	}

	void track_bar::get_thumb_rect(RECT * rc) const
	{
		get_thumb_rect(m_display_position, m_range_min, m_range_max, rc);
	}

	bool track_bar::get_hot() const
	{
		return m_thumb_hot;
	}

	void track_bar::update_hot_status(POINT pt)
	{
		RECT rc;
		get_thumb_rect(&rc);
		bool in_rect = PtInRect(&rc, pt) !=0;

		POINT pts = pt;
		MapWindowPoints(get_wnd(), HWND_DESKTOP, &pts, 1);

		bool b_in_wnd = WindowFromPoint(pts) == get_wnd();
		bool b_new_hot = in_rect && b_in_wnd;

		if (m_thumb_hot != b_new_hot)
		{
			m_thumb_hot = b_new_hot;
			if (m_thumb_hot)
				SetCapture(get_wnd());
			else if (GetCapture() == get_wnd() && !m_dragging)
				ReleaseCapture();
			RedrawWindow(get_wnd(), &rc, 0, RDW_INVALIDATE|/*RDW_ERASE|*/RDW_UPDATENOW/*|RDW_ERASENOW*/);
		}
	}

	void track_bar_impl::draw_background (HDC dc, const RECT * rc) const
	{
		HWND wnd_parent = GetParent(get_wnd());
		POINT pt = {0, 0}, pt_old = {0,0};
		MapWindowPoints(get_wnd(), wnd_parent, &pt, 1);
		OffsetWindowOrgEx(dc, pt.x, pt.y, &pt_old);
		SendMessage(wnd_parent, WM_ERASEBKGND,(WPARAM)dc, 0);
		SetWindowOrgEx(dc, pt_old.x, pt_old.y, 0);
	}

	void track_bar_impl::draw_thumb (HDC dc, const RECT * rc)
	{
		if (get_uxtheme_ptr() && get_theme_handle())
		{
			get_uxtheme_ptr()->DrawThemeBackground(get_theme_handle(), dc, TKP_THUMB, get_enabled() ? (get_hot() ? TUS_HOT : TUS_NORMAL) : TUS_DISABLED, rc, 0);
		}
		else
		{
			HPEN pn_highlight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHIGHLIGHT));
			HPEN pn_dkshadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW));
			HPEN pn_shadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));

			HPEN pn_old = SelectPen(dc, pn_highlight);

			MoveToEx(dc, rc->left, rc->top, 0);
			LineTo(dc, rc->right-1, rc->top);
			SelectPen(dc, pn_dkshadow);
			LineTo(dc, rc->right-1, rc->bottom-1);
			SelectPen(dc, pn_highlight);
			MoveToEx(dc, rc->left, rc->top, 0);
			LineTo(dc, rc->left, rc->bottom-1);
			SelectPen(dc, pn_dkshadow);
			LineTo(dc, rc->right, rc->bottom-1);

			SelectPen(dc, pn_shadow);
			MoveToEx(dc, rc->left+1, rc->bottom-2, 0);
			LineTo(dc, rc->right-1, rc->bottom-2);
			MoveToEx(dc, rc->right-2, rc->top+1, 0);
			LineTo(dc, rc->right-2, rc->bottom-2);
			SelectPen(dc, pn_old);

			DeleteObject(pn_highlight);
			DeleteObject(pn_shadow);
			DeleteObject(pn_dkshadow);

			RECT rc_fill = *rc;

			rc_fill.top+=1;
			rc_fill.left+=1;
			rc_fill.right-=2;
			rc_fill.bottom-=2;

			HBRUSH br = GetSysColorBrush(COLOR_BTNFACE);
			FillRect(dc, &rc_fill, br);
			if (!get_enabled())
			{
				COLORREF cr_btnhighlight = GetSysColor(COLOR_BTNHIGHLIGHT);
				int x, y;
				for (x=rc_fill.left; x<rc_fill.right; x++)
					for (y=rc_fill.top; y<rc_fill.bottom; y++)
						if ((x+y)%2)
							SetPixel(dc, x, y, cr_btnhighlight); //i dont have anything better than SetPixel
			}
		}
	}

	void track_bar_impl::draw_channel (HDC dc, const RECT * rc)
	{
		if (get_uxtheme_ptr() && get_theme_handle())
		{
			get_uxtheme_ptr()->DrawThemeBackground(get_theme_handle(), dc, TKP_TRACK, TUTS_NORMAL, rc, 0);
		}
		else
		{
			RECT rc_temp = *rc;
			DrawEdge (dc, &rc_temp, EDGE_SUNKEN, BF_RECT);
		}
	}

	const uxtheme_handle * track_bar::get_uxtheme_ptr() const
	{
		return m_uxtheme;
	}
	uxtheme_handle * track_bar::get_uxtheme_ptr()
	{
		return m_uxtheme;
	}
	HTHEME track_bar::get_theme_handle() const
	{
		return m_theme;
	}

	LRESULT WINAPI track_bar::on_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		switch(msg)
		{
		case WM_NCCREATE:
			break;
		case WM_CREATE:
			{
				uxtheme_handle::g_create(m_uxtheme);
				if (m_uxtheme && m_uxtheme->IsThemeActive() && m_uxtheme->IsAppThemed())
				{
					m_theme = m_uxtheme->OpenThemeData(wnd, L"Trackbar");
				}
			}
			break;
		case WM_THEMECHANGED:
			{
				if (m_uxtheme)
				{
					if (m_theme)
					{
						m_uxtheme->CloseThemeData(m_theme);
						m_theme=0;
					}
					if (m_uxtheme->IsThemeActive() && m_uxtheme->IsAppThemed())
						m_theme = m_uxtheme->OpenThemeData(wnd, L"Trackbar");
				}
			}
			break;
		case WM_DESTROY:
			{
				if (m_uxtheme)
				{
					if (m_theme) m_uxtheme->CloseThemeData(m_theme);
					m_theme=0;
				}
				delete m_uxtheme;
				m_uxtheme = 0;
			}
			break;
		case WM_NCDESTROY:
			break;
		case WM_SIZE:
			RedrawWindow(wnd, 0, 0, RDW_INVALIDATE|RDW_ERASE);
			break;
		case WM_MOUSEMOVE:
			{

					POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
					if (m_dragging)
					{
						if (get_enabled()) 
						{
							RECT rc_channel, rc_client;
							GetClientRect(wnd, &rc_client);
							get_channel_rect(&rc_channel);

							int cx = pt.x;
							if (cx < rc_channel.left) cx = rc_channel.left;
							else if (cx > rc_channel.right) cx = rc_channel.right;
							unsigned pos = MulDiv(cx - rc_channel.left, m_range_max - m_range_min, rc_channel.right-rc_channel.left) + m_range_min;
							set_position_internal(pos);
							if (m_wnd_tooltip && m_host)
							{
								POINT pts = pt;
								ClientToScreen(wnd, &pts);
								CString temp;
								m_host->get_tooltip_text(pos, temp);
								update_tooltip(pts, temp);
							}
							if (m_host)
								m_host->on_position_change(pos, true);
						}
					}
					else
					{
						update_hot_status(pt);
					}
			}
			break;
		case WM_ENABLE:
			{
				RECT rc;
				get_thumb_rect(&rc);
				InvalidateRect(wnd, &rc, TRUE);
			}
			break;
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDOWN:
			{
				if (m_dragging)
				{
					destroy_tooltip();
					if (GetCapture() == wnd)
						ReleaseCapture();
					SetFocus(IsWindow(m_wnd_prev) ? m_wnd_prev : FindParentPopup(wnd));
					m_dragging = false;
					set_position_internal(m_position);
				}
			}
			break;
		case WM_LBUTTONDOWN:
			{
				if (get_enabled()) 
				{
					POINT pt; 

					pt.x = GET_X_LPARAM(lp);
					pt.y = GET_Y_LPARAM(lp);

					RECT rc_channel, rc_client;
					GetClientRect(wnd, &rc_client);
					get_channel_rect(&rc_channel);

					if (PtInRect(&rc_client, pt))
					{
						m_dragging = true;
						SetCapture(wnd);
						int cx = pt.x;
						if (cx < rc_channel.left) cx = rc_channel.left;
						else if (cx > rc_channel.right) cx = rc_channel.right;

						//message hook would be better than this focus stealing stuff
						SetFocus(wnd);
						unsigned pos = MulDiv(cx - rc_channel.left, m_range_max - m_range_min, rc_channel.right-rc_channel.left) + m_range_min;
						set_position_internal(pos);
						POINT pts = pt;
						ClientToScreen(wnd, &pts);
						if (m_show_tooltips && m_host)
						{
							CString temp;
							m_host->get_tooltip_text(pos, temp);
							create_tooltip(temp, pts);
						}
						if (m_host)
							m_host->on_position_change(pos, true);
					}
				}
			}
			return 0;
		case WM_LBUTTONUP:
			{
				if (m_dragging)
				{
					destroy_tooltip();
					if (GetCapture() == wnd)
						ReleaseCapture();
					m_dragging = false;
					if (get_enabled()) 
					{
						POINT pt; 

						pt.x = GET_X_LPARAM(lp);
						pt.y = GET_Y_LPARAM(lp);

						RECT rc_channel, rc_client;
						GetClientRect(wnd, &rc_client);
						get_channel_rect(&rc_channel);

						int cx = pt.x;
						if (cx < rc_channel.left) cx = rc_channel.left;
						else if (cx > rc_channel.right) cx = rc_channel.right;
						unsigned pos = MulDiv(cx - rc_channel.left, m_range_max - m_range_min, rc_channel.right-rc_channel.left) + m_range_min;
						set_position_internal(pos);
					}
					SetFocus(IsWindow(m_wnd_prev) ? m_wnd_prev : FindParentPopup(wnd));
					if (m_host)
						m_host->on_position_change(m_display_position, false);
				}
			}
			return 0;
		case WM_KEYDOWN:
			if (wp == VK_ESCAPE)
			{
				destroy_tooltip();
				if (GetCapture() == wnd)
					ReleaseCapture();
				SetFocus(IsWindow(m_wnd_prev) ? m_wnd_prev : FindParentPopup(wnd));
				m_dragging = false;
				set_position_internal(m_position);
				return 0;
			}
			break;
		case WM_SETFOCUS:
			m_wnd_prev = (HWND)wp;
			break;
		case WM_ERASEBKGND:
			return 0;
		case WM_PAINT:
			{
				RECT rc_client;
				GetClientRect(wnd, &rc_client);

				PAINTSTRUCT ps;

				HDC dc = BeginPaint(wnd, &ps);

				RECT rc_thumb;

				get_thumb_rect(&rc_thumb);

				RECT rc_track; //channel
				get_channel_rect(&rc_track);

				//Offscreen rendering to eliminate flicker
				HDC dc_mem = CreateCompatibleDC(dc);

				//Create a rect same size of update rect
				HBITMAP bm_mem = CreateCompatibleBitmap(dc, rc_client.right, rc_client.bottom);

				HBITMAP bm_old = (HBITMAP)SelectObject(dc_mem, bm_mem);

				//we should always be erasing first, so shouldn't be needed
				BitBlt(dc_mem, 0, 0, rc_client.right, rc_client.bottom, dc, 0, 0, SRCCOPY);
				if (ps.fErase)
				{
					draw_background(dc_mem, &rc_client);
				}

				draw_channel(dc_mem, &rc_track);
				draw_thumb(dc_mem, &rc_thumb);

				BitBlt(dc, 0, 0, rc_client.right, rc_client.bottom, dc_mem, 0, 0, SRCCOPY);
				SelectObject(dc_mem, bm_old);
				DeleteObject(bm_mem);
				DeleteDC(dc_mem);
				EndPaint(wnd, &ps);
			}
			return 0;

		}
		return DefWindowProc(wnd, msg, wp, lp);
	}
}
