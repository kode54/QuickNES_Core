// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CAboutDlg : public CDialogImpl<CAboutDlg>
{
	CHyperLink m_link_blargg;
	CHyperLink m_link_aspiringsquire;
	CHyperLink m_link_kode54;

public:
	enum { IDD = IDD_ABOUTBOX };

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow( GetParent() );

		m_link_blargg.SetLabel( _T( "Emu_Nes v0.5.0 - Copyright 2005 Shay Green" ) );
		m_link_blargg.SetHyperLink( _T( "http://www.slack.net/~ant/" ) );
		m_link_blargg.SubclassWindow( GetDlgItem( IDC_LINK_BLARGG ) );

		m_link_aspiringsquire.SetLabel( _T( "RealityA.pal by AspiringSquire" ) );
		m_link_aspiringsquire.SetHyperLink( _T( "http://filespace.org/AspiringSquire/" ) );
		m_link_aspiringsquire.SubclassWindow( GetDlgItem( IDC_LINK_ASPIRINGSQUIRE ) );

		m_link_kode54.SetLabel( _T( "Windows front-end by Chris Moeller" ) );
		m_link_kode54.SetHyperLink( _T( "http://static.morbo.org/kode54/" ) );
		m_link_kode54.SubclassWindow( GetDlgItem( IDC_LINK_KODE54 ) );

		{
			OSVERSIONINFO ovi = { 0 };
			ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			BOOL bRet = ::GetVersionEx(&ovi);
			if ( bRet && ( ovi.dwMajorVersion >= 5 ) )
			{
				m_link_blargg.m_clrLink = GetSysColor( 26 /* COLOR_HOTLIGHT */ );
				m_link_blargg.m_clrVisited = m_link_blargg.m_clrLink;
				m_link_aspiringsquire.m_clrLink = m_link_blargg.m_clrLink;
				m_link_aspiringsquire.m_clrVisited = m_link_blargg.m_clrLink;
				m_link_kode54.m_clrLink = m_link_blargg.m_clrLink;
				m_link_kode54.m_clrVisited = m_link_blargg.m_clrLink;
			}
		}

		CString timestamp;
		timestamp = CString( _T( "Built: " ) ) + _T( __DATE__ ) + _T( ' ' ) + _T( __TIME__ ) + _T( " -0700" );
		SetDlgItemText( IDC_BUILD_TIME, timestamp );

		return TRUE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
