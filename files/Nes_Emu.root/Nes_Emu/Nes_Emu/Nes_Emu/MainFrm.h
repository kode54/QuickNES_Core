// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "display/display.h"

#include "sound/sound_out.h"
#include "sound/sound_config.h"

#include "input/input.h"

#include "config/config.h"

static const int top_clip = 8; // first scanlines not visible on most televisions
static const int bottom_clip = 4; // last scanlines ^

static const unsigned char nes_palette [64][4] =
{
   {0x60,0x60,0x60}, {0x00,0x21,0x7b}, {0x00,0x00,0x9c}, {0x31,0x00,0x8b},
   {0x59,0x00,0x6f}, {0x6f,0x00,0x31}, {0x64,0x00,0x00}, {0x4f,0x11,0x00},
   {0x2f,0x19,0x00}, {0x27,0x29,0x00}, {0x00,0x44,0x00}, {0x00,0x39,0x37},
   {0x00,0x39,0x4f}, {0x00,0x00,0x00}, {0x0c,0x0c,0x0c}, {0x0c,0x0c,0x0c},
   
   {0xae,0xae,0xae}, {0x10,0x56,0xce}, {0x1b,0x2c,0xff}, {0x60,0x20,0xec},
   {0xa9,0x00,0xbf}, {0xca,0x16,0x54}, {0xca,0x1a,0x08}, {0x9e,0x3a,0x04},
   {0x67,0x51,0x00}, {0x43,0x61,0x00}, {0x00,0x7c,0x00}, {0x00,0x71,0x53},
   {0x00,0x71,0x87}, {0x0c,0x0c,0x0c}, {0x0c,0x0c,0x0c}, {0x0c,0x0c,0x0c},
   
   {0xff,0xff,0xff}, {0x44,0x9e,0xfe}, {0x5c,0x6c,0xff}, {0x99,0x66,0xff},
   {0xd7,0x60,0xff}, {0xff,0x62,0x95}, {0xff,0x64,0x53}, {0xf4,0x94,0x30},
   {0xc2,0xac,0x00}, {0x90,0xc4,0x14}, {0x52,0xd2,0x28}, {0x20,0xc6,0x92},
   {0x18,0xba,0xd2}, {0x4c,0x4c,0x4c}, {0x0c,0x0c,0x0c}, {0x0c,0x0c,0x0c},
   
   {0xff,0xff,0xff}, {0xa3,0xcc,0xff}, {0xa4,0xb4,0xff}, {0xc1,0xb6,0xff},
   {0xe0,0xb7,0xff}, {0xff,0xc0,0xc5}, {0xff,0xbc,0xab}, {0xff,0xd0,0x9f},
   {0xfc,0xe0,0x90}, {0xe2,0xea,0x98}, {0xca,0xf2,0xa0}, {0xa0,0xea,0xe2},
   {0xa0,0xe2,0xfa}, {0xb6,0xb6,0xb6}, {0x0c,0x0c,0x0c}, {0x0c,0x0c,0x0c}
};

static bool file_picker( HWND w, TCHAR * & path, const TCHAR * title, const TCHAR * filter, const TCHAR * default_extension, bool save )
{
	if ( ! path )
	{
#ifdef UNICODE
		path = new TCHAR[ 32768 ];
#else
		path = new TCHAR[ MAX_PATH + 1 ];
#endif
		* path = 0;
	}

	size_t filter_len = _tcslen( filter ) + 3;
	TCHAR * real_filter = new TCHAR[ filter_len ];
	memset( real_filter, 0, filter_len * sizeof( TCHAR ) );
	_tcscpy( real_filter, filter );

	for ( size_t i = 0; i < filter_len; ++i )
	{
		if ( real_filter[ i ] == _T( '|' ) ) real_filter[ i ] = 0;
	}

	OPENFILENAME ofn;
	memset( &ofn, 0, sizeof( ofn ) );

	ofn.lStructSize = sizeof( ofn );
	ofn.hwndOwner = w;
	ofn.lpstrFilter = real_filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = path;
#ifdef UNICODE
	ofn.nMaxFile = 32767;
#else
	ofn.nMaxFile = MAX_PATH;
#endif
	ofn.lpstrTitle = title;
	ofn.lpstrDefExt = default_extension;

	if ( ! save )
	{
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

		if ( GetOpenFileName( & ofn ) )
		{
			delete [] real_filter;

			return true;
		}
	}
	else
	{
		ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

		if ( GetSaveFileName( & ofn ) )
		{
			delete [] real_filter;

			return true;
		}
	}

	delete [] real_filter;

	return false;
}

static CString format_time( unsigned seconds )
{
	std::tostringstream text;

	text << std::setw( 1 ) << std::setfill( _T( '0' ) );

	if ( seconds > 60 * 60 )
	{
		text << seconds / ( 60 * 60 ) << _T( ':' ) << std::setw( 2 )
			<< ( seconds / 60 ) % 60 << _T( ':' ) << std::setw( 2 )
			<< seconds % 60;
	}
	else
	{
		text << seconds / 60 << _T( ':' ) << std::setw( 2 )
			<< seconds % 60;
	}

	return CString( text.str().c_str() );
}

class CMainFrame : public CFrameWindowImpl<CMainFrame>, public CUpdateUI<CMainFrame>,
		public CMessageFilter, public CIdleHandler
{
	class bookmarks_t
	{
		unsigned bookmark[ 10 ];

	public:
		bookmarks_t()
		{
			reset();
		}

		unsigned operator [] ( unsigned n ) const
		{
			assert( n < 10 );
			return bookmark[ n ];
		}

		unsigned & operator [] ( unsigned n )
		{
			assert( n < 10 );
			return bookmark[ n ];
		}

		enum { invalid = ~0 };

		void reset()
		{
			for ( unsigned i = 0; i < ARRAYSIZE( bookmark ); ++i )
				bookmark[ i ] = invalid;
		}
	};

	bookmarks_t bookmarks;


	Nes_Recorder::equalizer_t default_eq;

	Nes_Rewinder m_emu;

	Effects_Buffer m_effects_buffer;

	CString save_path;

	CString filename_base;

	core_config_t core_config;

	sound_config_t sound_config;


	display * m_video;

	sound_out * m_audio;

	input * m_controls;

	signed short   * sound_buf;
	unsigned         sound_buf_size;

	unsigned input_last;

	TCHAR * path;
	TCHAR * path_snap;
	TCHAR * path_film;

	CString status_text;

	enum
	{
		emu_stopped,
		emu_paused,
		emu_running
	}
	emu_state;

	int emu_direction;

	int emu_step;

	// for trackbar
	bool emu_seeking;
	bool emu_was_paused;

public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CNes_EmuView m_view;

	CCommandBarXPCtrl m_CmdBar;

	CTrackBarCtrlX m_TrackBar;

	CToolTipCtrl m_ToolTip;
	CToolInfo    * m_ToolInfo;

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		UIUpdateToolBar();
		if ( emu_state == emu_stopped )
		{
			//m_TrackBar.ClearTics();
			m_TrackBar.SetRange( 0, 0 );
		}
		else if ( emu_state == emu_paused )
		{
			CString text;
			text.LoadString( IDS_PAUSED );
			if ( status_text != text )
			{
				CStatusBarCtrl status = m_hWndStatusBar;
				status.SetText( 0, status_text = text );
			}
		}
		BOOL state = emu_state != emu_stopped;
		UIEnable( ID_SNAP_LOAD, state );
		UIEnable( ID_SNAP_SAVE, state );
		UIEnable( ID_MOVIE_LOAD, state );
		UIEnable( ID_MOVIE_SAVE, state );
		UIEnable( ID_CORE_RESET, state );
		UIEnable( ID_CORE_PAUSE, state );
		UIEnable( ID_CORE_NEXTFRAME, state );
		UIEnable( ID_CORE_REWIND, state );
		m_TrackBar.EnableWindow( state );
		return FALSE;
	}

	BOOL IsRunning()
	{
		return ( emu_state == emu_running ) ? TRUE : FALSE;
	}

	void config_sound()
	{
		Nes_Recorder::equalizer_t eq = default_eq;
		if ( sound_config.effects_enabled )
		{
			// bass - logarithmic, 2 to 8194 Hz
			double bass = double( 255 - sound_config.bass ) / 255;
			eq.bass = std::pow( 2.0, bass * 13 ) + 2.0;

			// treble - level from -108 to 0 to 5 dB
			double treble = double( sound_config.treble - 128 ) / 128;
			eq.cutoff = 0;
			eq.treble = treble * ( ( treble > 0 ) ? 16.0 : 80.0 ) - 8.0;
		}
		m_emu.set_equalizer( eq );

		double depth = double( sound_config.echo_depth ) / 255;
		Effects_Buffer::config_t cfg;
		cfg.pan_1 = -0.6 * depth;
		cfg.pan_2 = 0.6 * depth;
		cfg.reverb_delay = 880 * 0.1;
		cfg.reverb_level = 0.5 * depth;
		cfg.echo_delay = 610 * 0.1;
		cfg.echo_level = 0.30 * depth;
		cfg.delay_variance = 180 * 0.1;
		cfg.effects_enabled = ( ( sound_config.effects_enabled ) && ( sound_config.echo_depth > 0 ) );
		m_effects_buffer.config( cfg );
	}

	void DoFrame()
	{
		if ( emu_state == emu_running )
		{
			const char * err = 0;

			BOOL cut = FALSE;

			if ( ! m_audio )
			{
				sound_buf_size = sound_config.sample_rate / ( m_emu.frames_per_second / 2 ) * 2;

				m_audio = create_sound_out();
				err = m_audio->open( sound_config.sample_rate, 2, sound_buf_size, 10 );
				if ( ! err ) err = m_emu.set_sample_rate( sound_config.sample_rate, & m_effects_buffer );
				if ( err )
				{
					stop_error( err );
					return;
				}

				config_sound();

				m_emu.enable_sound( true );

				if ( sound_buf )
				{
					delete [] sound_buf;
					sound_buf = 0;
				}

				sound_buf = new short[ sound_buf_size ];
			}

			unsigned input_now = 0;

			if ( m_controls )
			{
				m_controls->poll();

				emu_direction = m_controls->get_direction();

				input_now = m_controls->read();

				if ( input_now != input_last )
				{
					unsigned change = input_now ^ input_last;
					if ( input_now & change )
					{
						m_controls->set_direction( 1 );

						if ( m_emu.replaying() ) cut = TRUE;

						m_emu.start_recording();
					}

					input_last = input_now;
				}

				if ( emu_direction < 0 && m_emu.tell() <= m_emu.movie_begin() )
				{
					CString text;
					text.LoadString( IDS_PAUSED );
					if ( status_text != text )
					{
						CStatusBarCtrl status = m_hWndStatusBar;
						status.SetText( 0, status_text = text );
					}
					return;
				}
			}

			if ( m_video )
			{
				void * fb;
				unsigned pitch;
				err = m_video->lock_framebuffer( fb, pitch );
				if ( ! err )
				{
					m_emu.set_pixels( fb, pitch );
					if ( emu_direction < 0 )
						m_emu.prev_frame();
					else
						err = m_emu.next_frame( input_now & 255, ( input_now >> 8 ) & 255 );

					m_video->unlock_framebuffer();

					if ( err )
					{
						emu_state = emu_stopped;
					}
				}

				if ( m_controls )
				{
					if ( m_emu.were_joypads_read() ) m_controls->strobe();
				}

				if ( m_audio && ! emu_seeking )
				{
					unsigned samples_read = m_emu.read_samples( sound_buf, sound_buf_size );
					err = m_audio->write_frame( sound_buf, samples_read );
					if ( err )
					{
						stop_error( err );
						return;
					}
				}

				{
					unsigned char palette[ m_emu.palette_size ];
					for ( unsigned i = 0; i < m_emu.palette_size; ++i )
					{
						palette[ i ] = m_emu.palette_entry( i );
					}
					m_video->update_palette( ( const unsigned char * ) & nes_palette, palette, m_emu.palette_start, m_emu.palette_size );
				}

				RECT rect;
				rect.left = m_emu.image_left;
				rect.top = m_emu.get_buffer_y() + top_clip + m_emu.image_top;
				rect.right = rect.left + m_emu.image_width;
				rect.bottom = rect.top + m_emu.image_height - top_clip - bottom_clip;

				bool wait_for_vsync = true;

				if ( m_audio && m_audio->buffered() < 5 )
					wait_for_vsync = false;

				m_video->paint( rect, wait_for_vsync );
			}

			if ( ! emu_seeking )
			{
				m_TrackBar.SetRangeMin( m_emu.movie_begin() );
				m_TrackBar.SetRangeMax( m_emu.movie_end(), cut );
				m_TrackBar.SetPos( m_emu.tell() );
			}
			UISetCheck( ID_CORE_REWIND, emu_direction < 0 );

			{
				CString text;
				if ( emu_direction < 0 ) text.LoadString( IDS_REWINDING );
				else if ( m_emu.replaying() ) text.LoadString( IDS_PLAYING );
				else text.LoadString( IDS_RECORDING );
				if ( status_text != text )
				{
					CStatusBarCtrl status = m_hWndStatusBar;
					status.SetText( 0, status_text = text );
				}
			}

			if ( emu_step )
			{
				if ( --emu_step == 0 ) emu_state = emu_paused;
			}
		}
	}

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		//UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_SNAP_LOAD, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_SNAP_SAVE, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_MOVIE_LOAD, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_MOVIE_SAVE, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_CORE_RESET, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_CORE_PAUSE, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_CORE_NEXTFRAME, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_CORE_REWIND, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_CORE_RECORDINDEFINITELY, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		//MESSAGE_HANDLER(WM_SIZING, OnSizing)
		MESSAGE_HANDLER(WM_MOVE, OnMove)
		MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD, OnSnapLoad)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE, OnSnapSave)
		COMMAND_ID_HANDLER(ID_MOVIE_LOAD, OnMovieLoad)
		COMMAND_ID_HANDLER(ID_MOVIE_SAVE, OnMovieSave)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_0, OnSnapLoad_Q0)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_1, OnSnapLoad_Q1)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_2, OnSnapLoad_Q2)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_3, OnSnapLoad_Q3)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_4, OnSnapLoad_Q4)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_5, OnSnapLoad_Q5)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_6, OnSnapLoad_Q6)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_7, OnSnapLoad_Q7)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_8, OnSnapLoad_Q8)
		COMMAND_ID_HANDLER(ID_SNAP_LOAD_9, OnSnapLoad_Q9)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_0, OnSnapSave_Q0)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_1, OnSnapSave_Q1)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_2, OnSnapSave_Q2)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_3, OnSnapSave_Q3)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_4, OnSnapSave_Q4)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_5, OnSnapSave_Q5)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_6, OnSnapSave_Q6)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_7, OnSnapSave_Q7)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_8, OnSnapSave_Q8)
		COMMAND_ID_HANDLER(ID_SNAP_SAVE_9, OnSnapSave_Q9)
		COMMAND_ID_HANDLER(ID_CORE_RESET, OnCoreReset)
		COMMAND_ID_HANDLER(ID_CORE_PAUSE, OnCorePause)
		COMMAND_ID_HANDLER(ID_CORE_NEXTFRAME, OnCoreNextFrame)
		COMMAND_ID_HANDLER(ID_CORE_REWIND, OnCoreRewind)
		COMMAND_ID_HANDLER(ID_CORE_RECORDINDEFINITELY, OnCoreRecordIndefinitely)
		COMMAND_ID_HANDLER(ID_CONFIGUREINPUT, OnConfigureInput)
		COMMAND_ID_HANDLER(ID_CONFIGURESOUND, OnConfigureSound)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_HELP_CONTROLS, OnHelpControls)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	CMainFrame()
	{
		m_video = 0;

		m_audio = 0;

		sound_buf = 0;

		default_eq = m_emu.equalizer();
		
		emu_state = emu_stopped;
		
		path = 0;
		path_snap = 0;
		path_film = 0;

		m_ToolInfo = 0;
	}

	~CMainFrame()
	{
		if ( filename_base.GetLength() )
		{
			sram_save();
			filename_base.Empty();
		}

		if ( m_controls )
		{
			input_config_save();

			delete m_controls;
			m_controls = 0;
		}

		if ( m_audio )
		{
			delete m_audio;
			m_audio = 0;
		}

		if ( m_video )
		{
			delete m_video;
			m_video = 0;
		}

		if ( sound_buf )
		{
			delete [] sound_buf;
			sound_buf = 0;
		}

		if ( path_film )
		{
			delete [] path_film;
			path_film = 0;
		}

		if ( path_snap )
		{
			delete [] path_snap;
			path_snap = 0;
		}

		if ( path )
		{
			delete [] path;
			path = 0;
		}

		if ( m_ToolInfo )
		{
			delete m_ToolInfo;
			m_ToolInfo = 0;
		}

		if ( save_path.GetLength() )
		{
			core_config_save( core_config, save_path );
			sound_config_save( sound_config, save_path );
		}
	}

	bool init_save_path()
	{
		TCHAR path[ MAX_PATH + 1 ];
		if ( SUCCEEDED( SHGetFolderPath( 0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, 0, SHGFP_TYPE_CURRENT, path ) ) )
		{
			save_path = path;
			save_path += _T( "\\Nes_Emu" );
			if ( CreateDirectory( save_path, NULL ) || GetLastError() == ERROR_ALREADY_EXISTS )
			{
				save_path += _T( '\\' );
				CString temp = save_path + _T( "snap" );
				if ( CreateDirectory( temp, NULL ) || GetLastError() == ERROR_ALREADY_EXISTS )
				{
					temp = save_path + _T( "sram" );
					if ( CreateDirectory( temp, NULL ) || GetLastError() == ERROR_ALREADY_EXISTS )
					{
						core_config_load( core_config, save_path );
						sound_config_load( sound_config, save_path );

						return true;
					}
				}
			}
		}

		save_path.Empty();

		return false;
	}

	void input_config_load()
	{
		CString temp = save_path + _T( "input.cfg" );
		Std_File_Reader_u in;
		if ( ! in.open( temp ) )
		{
			m_controls->load( in );
		}
	}

	void input_config_save()
	{
		CString temp = save_path + _T( "input.cfg" );
		Std_File_Writer_u out;
		if ( ! out.open( temp ) )
		{
			m_controls->save( out );
		}
	}

	/*void update_bookmarks()
	{
		m_TrackBar.ClearTics( TRUE );
		for ( unsigned i = 0; i < 10; ++i )
		{
			if ( bookmarks[ i ] != bookmarks_t::invalid )
				m_TrackBar.SetTic( bookmarks[ i ] );
		}
	}*/

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// create command bar window
		HWND hWndCmdBar = m_CmdBar.Create( m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE );
		// attach menu
		m_CmdBar.AttachMenu( GetMenu() );
		// load command bar images
		//m_CmdBar.LoadImages(IDR_MAINFRAME);
		// remove old menu
		SetMenu( NULL );

		//HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

		HWND hWndTrackBar = m_TrackBar.Create( m_hWnd, WTL::CRect( 0, 0, 120, 18 ), NULL, WS_CHILD | WS_VISIBLE | TBS_BOTTOM | TBS_HORZ );

		m_ToolTip.Create( m_hWnd, CRect( CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT ), 0, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, WS_EX_TOPMOST );
		m_ToolTip.SetWindowPos( HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
		m_ToolInfo = new CToolInfo( TTF_TRANSPARENT | TTF_ABSOLUTE | TTF_TRACK, m_hWnd );
		m_ToolTip.AddTool( m_ToolInfo );

		m_TrackBar.EnableWindow( FALSE );

		m_TrackBar.SetLineSize( 0 );

		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		AddSimpleReBarBand(hWndCmdBar);
		AddSimpleReBarBand(hWndTrackBar);

		m_TrackBar.SetLineSize( 1 );

		CReBarCtrl rebar = m_hWndToolBar;
		rebar.LockBands( true );
		rebar.MaximizeBand( 0, TRUE );

		m_CmdBar.Prepare();

		CreateSimpleStatusBar( ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );

		status_text.Empty();

		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

		if ( ! init_save_path() )
		{
			CString error, message;
			error.LoadString( IDS_ERROR );
			message.LoadString( IDS_ERROR_CONFIG );
			MessageBox( message, error, MB_ICONERROR | MB_OK );
			PostMessage( WM_CLOSE );
			return 0;
		}

		if ( core_config.record_indefinitely )
			UISetCheck( ID_CORE_RECORDINDEFINITELY, 1 );

		if ( core_config.show_status_bar )
		{
			BOOL handled;
			OnViewStatusBar( 0, 0, 0, handled );
		}

		/*SetWindowPos( NULL, 0, 0, 640, 456, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE );
		RECT rcClient;
		m_view.GetClientRect( & rcClient );
		SetWindowPos( NULL, 0, 0, 640 + ( 640 - rcClient.right - rcClient.left ), 456 + ( 456 - rcClient.bottom - rcClient.top ), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE );*/

		m_video = create_display();
		const char * err = m_video->open( m_emu.buffer_width, m_emu.buffer_height, m_hWndClient );

		if ( err )
		{
			delete m_video;
			m_video = 0;
			CString error;
			error.LoadString( IDS_ERROR );
			MessageBox( CString( err ), error , MB_ICONERROR | MB_OK );
			PostMessage( WM_CLOSE );
			return 0;
		}

		m_controls = create_input();
		err = m_controls->open( ATL::_AtlBaseModule.GetModuleInstance(), m_hWnd );
		if ( err )
		{
			delete m_controls;
			m_controls = 0;
			stop_error( err );
		}
		else input_config_load();

		m_view.m_video = m_video;

		::SetPriorityClass( ::GetCurrentProcess(), HIGH_PRIORITY_CLASS );
		::SetThreadPriority( ::GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		return 0;
	}

	void UpdateLayout(BOOL bResizeBars = TRUE)
	{
		RECT rcw, rcc, rcs;
		GetClientRect( & rcw );

		rcc = rcw;

		// position bars and offset their dimensions
		UpdateBarsPosition( rcc, bResizeBars );

		rcs.left = 0;
		rcs.top = 0;
		rcs.right = 640;
		rcs.bottom = 456;

		if ( m_hWndClient != NULL )
		{
			CWindow client = m_hWndClient;
			::AdjustWindowRectEx( & rcs, client.GetStyle(), FALSE, client.GetExStyle() );
		}

		rcw.right += ( rcs.right - rcs.left ) - ( rcc.right - rcc.left );
		rcw.bottom += ( rcs.bottom - rcs.top ) - ( rcc.bottom - rcc.top );

		::AdjustWindowRectEx( & rcw, GetStyle(), FALSE, GetExStyle() );

		SetWindowPos( NULL, 0, 0, rcw.right - rcw.left, rcw.bottom - rcw.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE );

		CFrameWindowImpl<CMainFrame>::UpdateLayout( bResizeBars );

		if ( m_video ) m_video->update_position();
	}

	LRESULT OnMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if ( m_video ) m_video->update_position();

		return 0;
	}

	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if ( m_controls ) m_controls->set_focus( true );

		return 0;
	}

	LRESULT OnKillFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if ( m_controls ) m_controls->set_focus( false );

		return 0;
	}

	LRESULT OnHScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			switch ( LOWORD( wParam ) )
			{
			case TB_THUMBTRACK:
				{
					POINT pt;
					GetCursorPos( & pt );
					m_ToolTip.TrackPosition( pt.x + 15, pt.y + 24 );
					m_ToolTip.UpdateTipText( ( const TCHAR * ) format_time( m_TrackBar.GetPos() / m_emu.frames_per_second ), m_hWnd );
				}

				if ( ! emu_seeking )
				{
					emu_seeking = true;
					emu_was_paused = emu_state == emu_paused;

					m_ToolTip.TrackActivate( m_ToolInfo, TRUE );
				}

				m_emu.quick_seek( m_emu.get_film().constrain( m_TrackBar.GetPos() ) );
				emu_state = emu_running;
				emu_step = 1;
				break;

			case TB_THUMBPOSITION:
				if ( emu_seeking )
				{
					emu_seeking = false;
					emu_state = emu_was_paused ? emu_paused : emu_running;

					m_ToolTip.TrackActivate( m_ToolInfo, FALSE );
				}

				m_emu.quick_seek( m_emu.get_film().constrain( m_TrackBar.GetPos() ) );
				emu_step = 0;
				break;
			}
		}

		return 0;
	}

#if 0
	LRESULT OnSizing(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		RECT & r = * ( RECT * ) lParam;

		RECT rcClient;
		m_view.GetClientRect( & rcClient );

		int width = rcClient.right - rcClient.left;
		int height = rcClient.bottom - rcClient.top;

		int newwidth = ( height * 256 / 228 ) - width;
		int newheight = ( width * 228 / 256 ) - height;

		if ( ! newwidth && ! newheight ) return 0;

		switch ( wParam )
		{
		case WMSZ_TOP:
		case WMSZ_BOTTOM:
			r.left -= newwidth / 2;
			r.right += newwidth / 2;
			break;

		case WMSZ_LEFT:
		case WMSZ_RIGHT:
			r.top -= newheight / 2;
			r.bottom += newheight / 2;
			break;

		case WMSZ_TOPLEFT:
			if ( newwidth < 0 )
				r.top -= newheight;
			else
				r.left -= newwidth;
			break;

		case WMSZ_TOPRIGHT:
			if ( newwidth < 0 )
				r.top -= newheight;
			else
				r.right += newwidth;
			break;

		case WMSZ_BOTTOMLEFT:
			if ( newwidth < 0 )
				r.bottom += newheight;
			else
				r.left -= newwidth;
			break;

		case WMSZ_BOTTOMRIGHT:
			if ( newwidth < 0 )
				r.bottom += newheight;
			else
				r.right += newwidth;
			break;
		}

		return TRUE;
	}
#endif

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	void stop_error( const char * err )
	{
		emu_state = emu_stopped;
		CStatusBarCtrl status = m_hWndStatusBar;
		status.SetText( 0, CString( err ) );
		status_text.Empty();

		if ( m_audio )
		{
			delete m_audio;
			m_audio = 0;
		}

		if ( m_video )
		{
			m_video->clear();
		}

		filename_base.Empty();
	}

	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString title, filter, extension;
		title.LoadString( IDS_ROM_TITLE );
		filter.LoadString( IDS_ROM_FILTER );
		extension.LoadString( IDS_ROM_EXTENSION );
		if ( file_picker( m_hWnd, path, title, filter, extension, false ) )
		{
			emu_direction = 1;
			emu_step = 0;
			emu_seeking = false;
			emu_was_paused = false;
			UISetCheck( ID_CORE_PAUSE, 0 );
			UISetCheck( ID_CORE_REWIND, 0 );

			if ( m_audio )
			{
				delete m_audio;
				m_audio = 0;
			}

			input_last = 0;
			if ( m_controls ) m_controls->reset();

			if ( filename_base.GetLength() )
			{
				sram_save();
				filename_base.Empty();
			}

			bookmarks.reset();
			//m_TrackBar.ClearTics( TRUE );

			const char * err;
			Std_File_Reader_u in;
			err = in.open( path );
			if ( ! err ) err = m_emu.set_sample_rate_nonlinear( 44100 );
			if ( ! err ) err = m_emu.use_circular_film( 5 * 60 * m_emu.frames_per_second );
			if ( ! err ) err = m_emu.load_ines_rom( in );
			if ( ! err )
			{
				emu_state = emu_running;

				const TCHAR * root = _tcsrchr( path, _T( '\\' ) );
				if ( ! root ) root = path;
				else root += 1;
				filename_base = root;
				int dot = filename_base.ReverseFind( _T( '.' ) );
				if ( dot >= 0 ) filename_base = filename_base.Left( dot );

				sram_load();
			}

			if ( err ) stop_error( err );
		}

		return 0;
	}

	void sram_load()
	{
		CString temp = save_path + _T( "sram" ) + _T( '\\' ) + filename_base + _T( ".sav" );
		Std_File_Reader_u in;
		if ( ! in.open( temp ) )
		{
			m_emu.load_battery_ram( in );
		}
	}

	void sram_save()
	{
		CString temp = save_path + _T( "sram" ) + _T( '\\' ) + filename_base + _T( ".sav" );

		if ( m_emu.has_battery_ram() )
		{
			Std_File_Writer_u out;
			if ( ! out.open( temp ) )
			{
				m_emu.save_battery_ram( out );
				return;
			}
		}

		_tremove( temp );
	}

	void snap_load( const TCHAR * path )
	{
		Std_File_Reader_u in;
		const char * err = in.open( path );
		if ( ! err )
		{
			Nes_Film & film = m_emu.get_film();

			err = film.read( in );
			if ( ! err )
			{
				m_emu.set_film( & film, film.end() );
				input_last = 0;
				if ( m_controls ) m_controls->reset();
			}

			if ( err ) stop_error( err );
		}
	}

	void snap_load( unsigned snap )
	{
		if ( bookmarks[ snap ] != bookmarks_t::invalid )
		{
			if ( bookmarks[ snap ] >= m_emu.movie_begin() &&
				bookmarks[ snap ] <= m_emu.movie_end() )
				m_emu.seek( bookmarks[ snap ] );
			else
			{
				bookmarks[ snap ] = bookmarks_t::invalid;
				//update_bookmarks();
			}
		}
		else
		{
			CString temp = save_path + _T( "snap" ) + _T( '\\' ) + filename_base + _T( '.' );
			temp.Append( snap );
			temp += _T( ".sav" );
			snap_load( temp );
		}
	}

	void snap_save( const TCHAR * path )
	{
		Std_File_Writer_u out;
		if ( ! out.open( path ) )
		{
			const int size = m_emu.frames_per_second * 60 * 5;
			const int period = m_emu.frames_per_second * 60;
			const int end = m_emu.movie_end();
			int begin = end - size + 1;
			if ( begin < m_emu.movie_begin() || begin > m_emu.movie_end() )
				begin = m_emu.movie_begin();
			m_emu.get_film().write( out, period, begin, end );
		}
	}

	void snap_save( unsigned snap )
	{
		bookmarks[ snap ] = m_emu.tell();
		//update_bookmarks();

		CString temp = save_path + _T( "snap" ) + _T( '\\' ) + filename_base + _T( '.' );
		temp.Append( snap );
		temp += _T( ".sav" );
		snap_save( temp );
	}

	LRESULT OnSnapLoad(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			CString title, filter, extension;
			title.LoadString( IDS_SNAP_LOAD_TITLE );
			filter.LoadString( IDS_SNAP_FILTER );
			extension.LoadString( IDS_SNAP_EXTENSION );
			if ( file_picker( m_hWnd, path_snap, title, filter, extension, false ) )
			{
				snap_load( path_snap );
			}
		}

		return 0;
	}

	LRESULT OnSnapSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			CString title, filter, extension;
			title.LoadString( IDS_SNAP_SAVE_TITLE );
			filter.LoadString( IDS_SNAP_FILTER );
			extension.LoadString( IDS_SNAP_EXTENSION );
			if ( file_picker( m_hWnd, path_snap, title, filter, extension, true ) )
			{
				snap_save( path_snap );
			}
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q0(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( ( unsigned ) 0 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q1(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 1 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q2(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 2 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q3(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 3 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q4(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 4 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q5(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 5 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q6(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 6 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q7(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 7 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q8(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 8 );
		}

		return 0;
	}

	LRESULT OnSnapLoad_Q9(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_load( 9 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q0(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( ( unsigned ) 0 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q1(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 1 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q2(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 2 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q3(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 3 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q4(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 4 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q5(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 5 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q6(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 6 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q7(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 7 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q8(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 8 );
		}

		return 0;
	}

	LRESULT OnSnapSave_Q9(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			snap_save( 9 );
		}

		return 0;
	}

	LRESULT OnMovieLoad(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			CString title, filter, extension;
			title.LoadString( IDS_FILM_LOAD_TITLE );
			filter.LoadString( IDS_FILM_FILTER );
			extension.LoadString( IDS_FILM_EXTENSION );
			if ( file_picker( m_hWnd, path_film, title, filter, extension, false ) )
			{
				Std_File_Reader_u in;
				const char * err = in.open( path_film );
				if ( ! err )
				{
					Nes_Film & film = m_emu.get_film();

					err = film.read( in );
					if ( ! err )
					{
						input_last = 0;
						m_emu.set_film( & film );
						m_TrackBar.SetRangeMin( m_emu.movie_begin() );
						m_TrackBar.SetRangeMax( m_emu.movie_end(), TRUE );
						m_TrackBar.SetPos( m_emu.tell() );
					}
					else stop_error( err );
				}
			}
		}

		return 0;
	}

	LRESULT OnMovieSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			CString title, filter, extension;
			title.LoadString( IDS_FILM_SAVE_TITLE );
			filter.LoadString( IDS_FILM_FILTER );
			extension.LoadString( IDS_FILM_EXTENSION );
			if ( file_picker( m_hWnd, path_film, title, filter, extension, true ) )
			{
				Std_File_Writer_u out;
				if ( ! out.open( path_film ) )
				{
					m_emu.save_movie( out );
				}
			}
		}

		return 0;
	}

	LRESULT OnCoreReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			emu_direction = 1;
			emu_step = 0;
			emu_state = emu_running;
			emu_seeking = false;
			emu_was_paused = false;
			m_emu.reset();
			bookmarks.reset();
			//m_TrackBar.ClearTics( TRUE );
			UISetCheck( ID_CORE_PAUSE, 0 );
			UISetCheck( ID_CORE_REWIND, 0 );

			input_last = 0;
			if ( m_controls ) m_controls->reset();
		}

		return 0;
	}

	LRESULT OnCorePause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			emu_state = ( emu_state == emu_running ) ? emu_paused : emu_running;
			UISetCheck( ID_CORE_PAUSE, emu_state == emu_paused );

			if ( m_audio ) m_audio->pause( emu_state == emu_paused );
		}

		return 0;
	}

	LRESULT OnCoreNextFrame(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			++emu_step;
			emu_state = emu_running;
		}

		return 0;
	}

	LRESULT OnCoreRewind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( emu_state != emu_stopped )
		{
			emu_direction = -emu_direction;
			if ( m_controls ) m_controls->set_direction( emu_direction );
			UISetCheck( ID_CORE_REWIND, emu_direction < 0 );
		}

		return 0;
	}

	LRESULT OnCoreRecordIndefinitely(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		core_config.record_indefinitely = ! core_config.record_indefinitely;
		UISetCheck( ID_CORE_RECORDINDEFINITELY, core_config.record_indefinitely );

		return 0;
	}

	LRESULT OnConfigureSound(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( do_sound_config( ATL::_AtlBaseModule.GetModuleInstance(), m_hWnd, & sound_config ) )
		{
			if ( m_audio )
			{
				delete m_audio;
				m_audio = 0;
			}
		}

		return 0;
	}

	LRESULT OnConfigureInput(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( m_controls ) m_controls->configure( ATL::_AtlBaseModule.GetModuleInstance(), m_hWnd );

		return 0;
	}

#if 0
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		static BOOL bVisible = TRUE;	// initially visible
		bVisible = !bVisible;
		CReBarCtrl rebar = m_hWndToolBar;
		int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
		rebar.ShowBand(nBandIndex, bVisible);
		UISetCheck(ID_VIEW_TOOLBAR, bVisible);
		UpdateLayout();
		return 0;
	}
#endif

	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		core_config.show_status_bar = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, core_config.show_status_bar ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, core_config.show_status_bar );
		UpdateLayout();
		return 0;
	}

	LRESULT OnHelpControls( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CControlsDlg dlg;
		dlg.DoModal();
		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}
};
