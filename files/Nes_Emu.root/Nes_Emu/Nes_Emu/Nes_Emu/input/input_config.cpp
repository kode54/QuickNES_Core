#include "stdafx.h"

#include "input_config.h"

#include <windows.h>
#include <tchar.h>

#include <commctrl.h>

#include "bind_list.h"
#include "dinput.h"
#include "guid_container.h"

#include "../resource.h"

static const TCHAR * item_roots[] = { _T( "Pad 1" ), _T( "Pad 2" ), _T( "Commands" ) };

static const TCHAR * item_pad[] = { _T( "A" ), _T( "B" ), _T( "Select" ), _T( "Start" ), _T( "Up" ), _T( "Down" ), _T( "Left" ), _T( "Right" ) };

static const TCHAR * item_command[] = { _T( "Rewind (toggle)" ), _T( "Rewind (hold)" ), _T("Fast forward (toggle)"), _T("Fast forward (hold)"), _T( "Toggle pad 1 rapid fire" ), _T( "Toggle pad 2 rapid fire" ) };

class input_config
{
	dinput                 * di;

	bind_list              * bl;

	bool                     have_event;
	dinput::di_event         last_event;

	std::vector< HTREEITEM > tree_items;

	// returned to list view control
	std::tstring             temp;

	// needed by WM_INITDIALOG
	void                   * di8;
	guid_container         * guids;

public:
	input_config()
	{
		di = 0;

		have_event = false;
	}

	~input_config()
	{
		if ( di )
		{
			delete di;
			di = 0;
		}
	}

	bool run( void * di8, void * hinstance, void * hwnd, guid_container * guids, bind_list * bl )
	{
		this->bl = bl;
		this->di8 = di8;
		this->guids = guids;

		di = create_dinput();
		
		return !! DialogBoxParam( ( HINSTANCE ) hinstance, MAKEINTRESOURCE( IDD_INPUT_CONFIG ), ( HWND ) hwnd, g_dlg_proc, ( LPARAM ) this );
	}

private:
	static INT_PTR CALLBACK g_dlg_proc( HWND w, UINT msg, WPARAM wp, LPARAM lp )
	{
		input_config * p_this;
	
		if(	msg == WM_INITDIALOG )
		{
			p_this = ( input_config * ) lp;
			SetWindowLongPtr( w, DWLP_USER, ( LONG_PTR ) p_this );
		}
		else
			p_this = reinterpret_cast< input_config * >( GetWindowLongPtr( w, DWLP_USER ) );
	
		return p_this ? p_this->DlgProc( w, msg, wp, lp ) : FALSE;
	}

public:
	INT_PTR DlgProc( HWND w, UINT msg, WPARAM wp, LPARAM lp )
	{
		switch ( msg )
		{
		case WM_INITDIALOG:
			{
				if ( di->open( di8, w, guids ) )
				{
					EndDialog( w, 0 );
					break;
				}

				di->set_focus( true );

				HWND wi = GetDlgItem( w, IDC_LIST_ASSIGN );

				{
					{
						LVCOLUMN lvc;
						memset( & lvc, 0, sizeof( lvc ) );

						lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
						lvc.fmt = LVCFMT_LEFT;
						lvc.cx = 260;
						lvc.pszText = _T( "Event" );
						lvc.iSubItem = 0;

						if ( SendMessage( wi, LVM_INSERTCOLUMN, 0, ( LPARAM ) & lvc ) == -1 ) break;

						lvc.cx = 150;
						lvc.pszText = _T( "Action" );
						lvc.iSubItem = 1;

						if ( SendMessage( wi, LVM_INSERTCOLUMN, 1, ( LPARAM ) & lvc ) == -1 ) break;
					}

					dinput::di_event e;
					unsigned         action;

					for ( unsigned i = 0, j = bl->get_count(); i < j; ++i )
					{
						bl->get( i, e, action );
						add_to_list( wi, e, action );
					}
				}
				
				wi = GetDlgItem( w, IDC_TREE_ACTIONS );

				{
					HTREEITEM item;

					TVINSERTSTRUCT tvi;
					memset( & tvi, 0, sizeof( tvi ) );

					tvi.hParent = TVI_ROOT;
					tvi.hInsertAfter = TVI_ROOT;
					tvi.item.mask = TVIF_TEXT;
					tvi.item.pszText = ( TCHAR * ) item_roots[ 0 ];

					item = ( HTREEITEM ) SendMessage( wi, TVM_INSERTITEM, 0, ( LPARAM ) & tvi ); if ( ! item ) break;

					tvi.hParent = item;
					tvi.hInsertAfter = TVI_LAST;

					for ( unsigned i = 0; i < 8; ++i )
					{
						tvi.item.pszText = ( TCHAR * ) item_pad[ i ];
						tvi.item.lParam = i;

						item = ( HTREEITEM ) SendMessage( wi, TVM_INSERTITEM, 0, ( LPARAM ) & tvi ); if ( ! item ) break;

						tree_items.push_back( item );
					}

					tvi.hParent = TVI_ROOT;
					tvi.hInsertAfter = TVI_ROOT;
					tvi.item.pszText = ( TCHAR * ) item_roots[ 1 ];

					item = ( HTREEITEM ) SendMessage( wi, TVM_INSERTITEM, 0, ( LPARAM ) & tvi ); if ( ! item ) break;

					tvi.hParent = item;
					tvi.hInsertAfter = TVI_LAST;

					for ( unsigned i = 0; i < 8; ++i )
					{
						tvi.item.pszText = ( TCHAR * ) item_pad[ i ];

						item = ( HTREEITEM ) SendMessage( wi, TVM_INSERTITEM, 0, ( LPARAM ) & tvi ); if ( ! item ) break;

						tree_items.push_back( item );
					}

					tvi.hParent = TVI_ROOT;
					tvi.hInsertAfter = TVI_ROOT;
					tvi.item.pszText = ( TCHAR * ) item_roots[ 2 ];

					item = ( HTREEITEM ) SendMessage( wi, TVM_INSERTITEM, 0, ( LPARAM ) & tvi ); if ( ! item ) break;

					tvi.hParent = item;
					tvi.hInsertAfter = TVI_LAST;

					for ( unsigned i = 0; i < 6; ++i )
					{
						tvi.item.pszText = ( TCHAR * ) item_command[ i ];

						item = ( HTREEITEM ) SendMessage( wi, TVM_INSERTITEM, 0, ( LPARAM ) & tvi ); if ( ! item ) break;

						tree_items.push_back( item );
					}
				}

				SetFocus( GetDlgItem( w, IDC_EDIT_EVENT ) );

				SetTimer( w, 0, 10, 0 );
			}
			break;

		case WM_SYSCOMMAND:
			if ( wp == SC_KEYMENU )
			{
				SetWindowLongPtr( w, DWLP_MSGRESULT, 0 );
				return TRUE;
			}
			break;

		case WM_TIMER:
			if ( wp == 0 )
			{
				if ( process_events( di->read() ) )
				{
					std::tostringstream event_text;
					format_event( last_event, event_text );

					SetWindowText( GetDlgItem( w, IDC_EDIT_EVENT ), event_text.str().c_str() );
				}
				SetWindowLongPtr( w, DWLP_MSGRESULT, 0 );
				return TRUE;
			}
			break;

		case DM_GETDEFID:
			SetWindowLongPtr( w, DWLP_MSGRESULT, 0 );
			return TRUE;
			break;

		case WM_COMMAND:
			switch ( wp )
			{
			case IDC_OK:
				KillTimer( w, 0 );
				EndDialog( w, 1 );
				break;

			case IDC_CANCEL:
				KillTimer( w, 0 );
				EndDialog( w, 1 );
				break;

			case IDC_ADD:
				if ( have_event )
				{
					HWND wi = GetDlgItem( w, IDC_TREE_ACTIONS );

					TVITEM tvi;
					memset( & tvi, 0, sizeof( tvi ) );

					tvi.mask = TVIF_HANDLE | TVIF_STATE;

					std::vector< HTREEITEM >::iterator it;
					for ( it = tree_items.begin(); it < tree_items.end(); ++it )
					{
						tvi.hItem = *it;
						tvi.stateMask = TVIS_SELECTED;
						
						if ( SendMessage( wi, TVM_GETITEM, 0, ( LPARAM ) & tvi ) )
						{
							if ( tvi.state & TVIS_SELECTED )
							{
								unsigned action = it - tree_items.begin();
								bl->add( last_event, action );
								add_to_list( GetDlgItem( w, IDC_LIST_ASSIGN ), last_event, action );
								break;
							}
						}
					}
				}
				break;

			case IDC_REMOVE:
				{
					HWND wi = GetDlgItem( w, IDC_LIST_ASSIGN );

					int n = ListView_GetSelectionMark( wi );

					if ( n != -1 )
					{
						bl->remove( n );
						remove_from_list( wi, n );
					}
				}
				break;
			}
			break;
		}

		return FALSE;
	}

private:
	void format_event( const dinput::di_event & e, std::tostringstream & out )
	{
		if ( e.type == dinput::di_event::ev_key )
		{
			unsigned scancode = e.key.which;
			if ( scancode == 0x45 || scancode == 0xC5 ) scancode ^= 0x80;
			scancode = ( ( scancode & 0x7F ) << 16 ) | ( ( scancode & 0x80 ) << 17 );
			TCHAR text[ 32 ];
			if ( GetKeyNameText( scancode, text, 32 ) )
				out << text;
		}
		else if ( e.type == dinput::di_event::ev_joy )
		{
			out << di->get_joystick_name( e.joy.serial );
			out << _T( ' ' );

			if ( e.joy.type == dinput::di_event::joy_axis )
			{
				out << _T( "axis " ) << e.joy.which << _T( ' ' );
				if ( e.joy.axis == dinput::di_event::axis_negative ) out << _T( '-' );
				else out << _T( '+' );
			}
			else if ( e.joy.type == dinput::di_event::joy_button )
			{
				out << _T( "button " ) << e.joy.which;
			}
			else if ( e.joy.type == dinput::di_event::joy_pov )
			{
				out << _T( "pov " ) << e.joy.which << _T( " " ) << e.joy.pov_angle << _T( '°' );
			}
		}
	}

	bool process_events( std::vector< dinput::di_event > events )
	{
		std::vector< dinput::di_event >::iterator it;
		bool     again;
		unsigned last = 0;
		do
		{
			again = false;
			for ( it = events.begin() + last; it < events.end(); ++it )
			{
				if ( it->type == dinput::di_event::ev_key )
				{
					if ( it->key.type == dinput::di_event::key_up )
					{
						again = true;
						last = it - events.begin();
						events.erase( it );
						break;
					}
				}
				else if ( it->type == dinput::di_event::ev_joy )
				{
					if ( it->joy.type == dinput::di_event::joy_axis )
					{
						if ( it->joy.axis == dinput::di_event::axis_center )
						{
							again = true;
							last = it - events.begin();
							events.erase( it );
							break;
						}
					}
					else if ( it->joy.type == dinput::di_event::joy_button )
					{
						if ( it->joy.button == dinput::di_event::button_up )
						{
							again = true;
							last = it - events.begin();
							events.erase( it );
							break;
						}
					}
					else if ( it->joy.type == dinput::di_event::joy_pov )
					{
						if ( it->joy.pov_angle == ~0 )
						{
							again = true;
							last = it - events.begin();
							events.erase( it );
							break;
						}
					}
				}
			}
		}
		while ( again );

		it = events.end();
		if ( it > events.begin() )
		{
			--it;
			have_event = true;
			last_event = *it;
			return true;
		}

		return false;
	}

	void add_to_list( HWND w, const dinput::di_event & e, unsigned action )
	{
		std::tstring str;

		{
			std::tostringstream text;
			format_event( e, text );
			str = text.str();
		}

		LVITEM lvi;
		memset( & lvi, 0, sizeof( lvi ) );

		lvi.mask = LVIF_TEXT;

		lvi.iItem = INT_MAX;
		lvi.pszText = ( TCHAR * ) str.c_str();

		int item = ListView_InsertItem( w, & lvi );

		if ( item != -1 )
		{
			str.clear();

			if ( action >= 0 && action <= 15 )
			{
				if ( action <= 7 ) str = item_roots[ 0 ];
				else str = item_roots[ 1 ];

				str += _T( ' ' );

				str += item_pad[ action & 7 ];
			}
			else if ( action >= 16 && action <= 21 ) str = item_command[ action - 16 ];

			lvi.iSubItem = 1;
			lvi.pszText = ( TCHAR * ) str.c_str();

			SendMessage( w, LVM_SETITEMTEXT, ( WPARAM ) item, ( LPARAM ) ( LVITEM * ) & lvi );
		}
	}

	void remove_from_list( HWND w, unsigned n )
	{
		ListView_DeleteItem( w, n );
	}
};

bool do_input_config( void * di8, void * hinstance, void * hwnd, guid_container * guids, bind_list * bl )
{
	input_config ic;

	return ic.run( di8, hinstance, hwnd, guids, bl );
}