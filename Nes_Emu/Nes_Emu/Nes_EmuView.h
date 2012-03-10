// Nes_EmuView.h : interface of the CNes_EmuView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "display/display.h"

class CNes_EmuView : public CWindowImpl<CNes_EmuView>
{
public:
	display * m_video;

	CNes_EmuView() : m_video( 0 ) {}

	DECLARE_WND_CLASS(NULL)

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		pMsg;
		return FALSE;
	}

	BEGIN_MSG_MAP(CNes_EmuView)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnClick)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnClick)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CPaintDC dc(m_hWnd);

		//TODO: Add your drawing code here
		if ( m_video )
		{
			m_video->repaint();
		}

		return 0;
	}

	LRESULT OnClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		::SetFocus( GetParent() );

		return 0;
	}
};
