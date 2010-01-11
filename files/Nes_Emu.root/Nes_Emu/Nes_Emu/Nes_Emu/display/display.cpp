#include "stdafx.h"
#include "display.h"

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <ddraw.h>

#pragma comment( lib, "ddraw.lib" )
#pragma comment( lib, "dxguid.lib" )
#pragma comment( lib, "winmm.lib" )

class display_i_ddraw : public display
{
	HWND                 hWnd;

	RECT                 rcWindow;
	RECT                 rcLast;

	LPDIRECTDRAW7        lpDD;
	LPDIRECTDRAWSURFACE7 lpsPrimary;
	LPDIRECTDRAWSURFACE7 lpsFramebuffer;
	LPDIRECTDRAWCLIPPER  lpDDClipper;

	LPDIRECTDRAWPALETTE  lppPalette;

	/*LPVOID               lpSurface;
	unsigned             dwPitch;*/

	unsigned             surface_width, surface_height, bit_depth;
	
	unsigned             red_mask, green_mask, blue_mask;

	unsigned char      * lpFakeSurface;

	unsigned char        palette[ 256 ][ 4 ];

	volatile unsigned    wait_lastscanline;
	volatile unsigned    wait_screenheight;
	volatile unsigned    wait_firstline;
	volatile bool        wait_used;
	HANDLE               wait_event;
	UINT                 wait_timerres;
	UINT                 wait_timerid;

public:
	display_i_ddraw()
	{
		hWnd = 0;

		memset( & rcWindow, 0, sizeof( rcWindow ) );

		lpDD = 0;
		lpsPrimary = 0;
		lpsFramebuffer = 0;
		lpDDClipper = 0;

		lppPalette = 0;

		lpFakeSurface = 0;

		wait_event = 0;
		wait_timerres = 0;
		wait_timerid = 0;
	}

	virtual ~display_i_ddraw()
	{
		if ( lpFakeSurface )
		{
			delete [] lpFakeSurface;
			lpFakeSurface = 0;
		}

		if ( lppPalette )
		{
			lppPalette->Release();
			lppPalette = 0;
		}

		if ( wait_timerid )
		{
			StopTimer();
		}

		if ( wait_event )
		{
			CloseHandle( wait_event );
			wait_event = 0;
		}

		if ( lpsFramebuffer )
		{
			lpsFramebuffer->Release();
			lpsFramebuffer = 0;
		}

		if ( lpDDClipper )
		{
			lpDDClipper->Release();
			lpDDClipper = 0;
		}

		if ( lpsPrimary )
		{
			lpsPrimary->Release();
			lpsPrimary = 0;
		}

		if ( lpDD )
		{
			lpDD->Release();
			lpDD = 0;
		}
	}

	virtual const char* open( unsigned buffer_width, unsigned buffer_height, HWND hWnd )
	{
		if ( this->hWnd )
			return "Display already open";

		this->hWnd = hWnd;

		update_position();

		if ( DirectDrawCreateEx( NULL, ( void ** ) & lpDD, IID_IDirectDraw7, NULL ) != DD_OK )
			return "Creating DirectDraw object";

		if ( lpDD->SetCooperativeLevel( hWnd, DDSCL_NORMAL ) != DD_OK )
			return "Setting DirectDraw cooperative level";

		DDSURFACEDESC2 ddsd;

		memset( &ddsd, 0, sizeof( ddsd ) );
		ddsd.dwSize = sizeof( ddsd );
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

		if ( lpDD->CreateSurface( &ddsd, & lpsPrimary, 0 ) != DD_OK )
			return "Creating primary surface";

		DDPIXELFORMAT format;
		format.dwSize = sizeof(DDPIXELFORMAT);
		
		if ( lpsPrimary->GetPixelFormat( & format ) != DD_OK )
			return "Querying pixel format";

		bit_depth = format.dwRGBBitCount;
		
		red_mask = format.dwRBitMask;
		green_mask = format.dwGBitMask;
		blue_mask = format.dwBBitMask;

		if ( lpDD->CreateClipper( 0, & lpDDClipper, 0 ) != DD_OK )
			return "Creating clipper";

		if ( lpDDClipper->SetHWnd( 0, hWnd ) != DD_OK )
			return "Setting clipper window";

		if ( lpsPrimary->SetClipper( lpDDClipper ) != DD_OK )
			return "Assigning clipper";

		if ( bit_depth == 8 )
		{
			LPPALETTEENTRY pal = new PALETTEENTRY[ 256 ];

			for ( unsigned i = 0; i < 10; ++i )
			{
				pal[ i ].peFlags = PC_EXPLICIT;
				pal[ i ].peRed = i;
			}
			for ( unsigned i = 10; i < 246; ++i )
			{
				pal[ i ].peFlags = PC_NOCOLLAPSE;
				pal[ i ].peRed = 0;
				pal[ i ].peGreen = 0;
				pal[ i ].peBlue = 0;
			}
			for ( unsigned i = 246; i < 256; ++i )
			{
				pal[ i ].peFlags = PC_EXPLICIT;
				pal[ i ].peRed = i;
			}

			HRESULT hr = lpDD->CreatePalette( DDPCAPS_8BIT | DDPCAPS_ALLOW256, pal, & lppPalette, 0 );

			delete [] pal;

			if ( hr != DD_OK )
				return "Creating palette";

			if ( lpsPrimary->SetPalette( lppPalette ) != DD_OK )
				return "Assigning palette to primary surface";
		}

		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		ddsd.dwWidth = 256 + 4;
		ddsd.dwHeight = 240 + 4;

		if ( lpDD->CreateSurface( & ddsd, & lpsFramebuffer, NULL ) != DD_OK )
			return "Creating offscreen surface";

		clear();

		// hee
		rcLast.left = 0;
		rcLast.top = 0;
		rcLast.right = 1;
		rcLast.bottom = 1;

		wait_event = CreateEvent( NULL, FALSE, FALSE, NULL );

		if ( ! StartTimer() )
		{
			return "Starting timer";
		}

		surface_width = buffer_width;
		surface_height = buffer_height;

		lpFakeSurface = new unsigned char[ surface_width * surface_height ];

		return 0;
	}

	virtual void update_position()
	{
		POINT pt = { 0, 0 };
		ClientToScreen( hWnd, & pt );
		GetClientRect( hWnd, & rcWindow );
		OffsetRect( & rcWindow, pt.x, pt.y );

		wait_screenheight = GetSystemMetrics( SM_CYSCREEN );
		wait_firstline = min( rcWindow.bottom, wait_screenheight );
		wait_lastscanline = 0;
		wait_used = false;
	}

	virtual const char* lock_framebuffer( void *& buffer, unsigned & pitch )
	{
		if ( lpsFramebuffer == 0 )
			return "No surface to lock";

		buffer = lpFakeSurface;
		pitch = surface_width;

		return 0;
	}

	virtual const char* unlock_framebuffer()
	{
		if ( lpsFramebuffer == 0 )
			return "No surface to unlock";

		return 0;
	}

	virtual const char* update_palette( const unsigned char * source_pal, const unsigned char * source_list, unsigned color_first, unsigned color_count )
	{
		switch ( bit_depth )
		{
		case 8:
			for ( unsigned i = 0; i < color_count; ++i )
			{
				const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
				LPPALETTEENTRY dst = &( ( LPPALETTEENTRY ) palette )[ i ];
				dst->peFlags = PC_RESERVED;
				dst->peRed = src[ 0 ];
				dst->peGreen = src[ 1 ];
				dst->peBlue = src[ 2 ];
			}

			if ( lppPalette->SetEntries( 0, color_first, color_count, ( ( LPPALETTEENTRY ) palette ) ) != DD_OK )
				return "Updating palette";

			break;

		case 16:
			switch ( green_mask )
			{
			case 0x3E0:
				switch ( red_mask )
				{
				case 0x7C00:
					for ( unsigned i = 0, j = color_first; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned color = ( src[ 0 ] & 0xF8 ) << 7;
						color |= ( src[ 1 ] & 0xF8 ) << 2;
						color |= ( src[ 2 ] & 0xF8 ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					for ( unsigned i = 0, j = color_first + 256; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned element = ( src[ 0 ] + 4 ) & ~7;
						unsigned color = ( min( element, 0xF8 ) ) << 7;
						element = ( src[ 1 ] + 4 ) & ~7;
						color |= ( min( element, 0xF8 ) ) << 2;
						element = ( src[ 2 ] + 4 ) & ~7;
						color |= ( min( element, 0xF8 ) ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					break;

				case 0x1F:
					for ( unsigned i = 0, j = color_first; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned color = ( src[ 2 ] & 0xF8 ) << 7;
						color |= ( src[ 1 ] & 0xF8 ) << 2;
						color |= ( src[ 0 ] & 0xF8 ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					for ( unsigned i = 0, j = color_first + 256; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned element = ( src[ 2 ] + 4 ) & ~7;
						unsigned color = ( min( element, 0xF8 ) ) << 7;
						element = ( src[ 1 ] + 4 ) & ~7;
						color |= ( min( element, 0xF8 ) ) << 2;
						element = ( src[ 0 ] + 4 ) & ~7;
						color |= ( min( element, 0xF8 ) ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					break;
				}
				break;

			case 0x7E0:
				switch ( red_mask )
				{
				case 0xF800:
					for ( unsigned i = 0, j = color_first; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned color = ( src[ 0 ] & 0xF8 ) << 8;
						color |= ( src[ 1 ] & 0xFC ) << 3;
						color |= ( src[ 2 ] & 0xF8 ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					for ( unsigned i = 0, j = color_first + 256; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned element = ( src[ 0 ] + 4 ) & ~7;
						unsigned color = ( min( element, 0xF8 ) ) << 8;
						element = ( src[ 1 ] + 2 ) & ~3;
						color |= ( min( element, 0xFC ) ) << 3;
						element = ( src[ 2 ] + 4 ) & ~7;
						color |= ( min( element, 0xF8 ) ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					break;

				case 0x1F:
					for ( unsigned i = 0, j = color_first; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned color = ( src[ 2 ] & 0xF8 ) << 8;
						color |= ( src[ 1 ] & 0xFC ) << 3;
						color |= ( src[ 0 ] & 0xF8 ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					for ( unsigned i = 0, j = color_first + 256; i < color_count; ++i, ++j )
					{
						const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
						unsigned element = ( src[ 2 ] + 4 ) & ~7;
						unsigned color = ( min( element, 0xF8 ) ) << 8;
						element = ( src[ 1 ] + 2 ) & ~3;
						color |= ( min( element, 0xFC ) ) << 3;
						element = ( src[ 0 ] + 4 ) & ~7;
						color |= ( min( element, 0xF8 ) ) >> 3;
						( ( unsigned short * ) &palette )[ j ] = ( unsigned short ) color;
					}
					break;
				}
				break;
			}
			break;

		case 24:
		case 32:
			if ( red_mask == 0xFF )
			{
				for ( unsigned i = 0, j = color_first; i < color_count; ++i, ++j )
				{
					const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
					palette[ j ][ 0 ] = src[ 0 ];
					palette[ j ][ 1 ] = src[ 1 ];
					palette[ j ][ 2 ] = src[ 2 ];
					palette[ j ][ 3 ] = 0;
				}
			}
			else
			{
				for ( unsigned i = 0, j = color_first; i < color_count; ++i, ++j )
				{
					const unsigned char * src = &source_pal[ source_list[ i ] * 4 ];
					palette[ j ][ 0 ] = src[ 2 ];
					palette[ j ][ 1 ] = src[ 1 ];
					palette[ j ][ 2 ] = src[ 0 ];
					palette[ j ][ 3 ] = 0;
				}
			}
			break;
		}

		return 0;
	}

	virtual const char* paint( RECT rcSource, bool wait )
	{
		if ( lpsFramebuffer == 0 )
			return "No framebuffer to paint from";

		/*if ( lpSurface != 0 )
			return "Framebuffer locked";*/

		LPVOID lpSurface;
		unsigned dwPitch;

		{
			DDSURFACEDESC2 ddsd;
			memset( &ddsd, 0, sizeof( ddsd ) );
			ddsd.dwSize = sizeof( ddsd );
			ddsd.dwFlags = DDSD_LPSURFACE | DDSD_PITCH;

			HRESULT hRes = lpsFramebuffer->Lock( 0, &ddsd, DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK | DDLOCK_SURFACEMEMORYPTR, 0 );

			if ( hRes == DDERR_SURFACELOST )
			{
				if ( lpsFramebuffer->Restore() != DD_OK )
					return "Surface lost";

				return 0;
			}

			if ( hRes != DD_OK )
				return "Locking surface";

			lpSurface = ddsd.lpSurface;
			dwPitch = ddsd.lPitch;
		}

		unsigned r = 0;

		for ( int y = rcSource.top; y < rcSource.bottom; ++y )
		{
			unsigned char * src_row = ( ( unsigned char * ) lpFakeSurface ) + y * surface_width;
			unsigned char * dst_row = ( ( unsigned char * ) lpSurface ) + ( y - rcSource.top ) * dwPitch;

			switch ( bit_depth )
			{
			case 8:
				memcpy( dst_row, src_row + rcSource.left, rcSource.right - rcSource.left );
				break;

			case 16:
				for ( int x1 = 0, x2 = rcSource.left; x2 < rcSource.right; x1 += 2, ++x2 )
				{
					unsigned index = src_row[ x2 ];
					if ( ! r ) r = rand();
					if ( r & 1 ) index += 256;
					( ( unsigned short * ) ( dst_row + x1 ) )[ 0 ] = ( ( unsigned short * ) palette )[ index ];
					r >>= 1;
				}
				break;
				
			case 24:
				for ( int x1 = 0, x2 = rcSource.left; x2 < rcSource.right; x1 += 3, ++x2 )
				{
					unsigned char * color = &palette[ src_row[ x2 ] ][ 0 ];
					dst_row[ x1 + 0 ] = color[ 0 ];
					dst_row[ x1 + 1 ] = color[ 1 ];
					dst_row[ x1 + 2 ] = color[ 2 ];
				}
				break;

			case 32:
				for ( int x1 = 0, x2 = rcSource.left; x2 < rcSource.right; x1 += 4, ++x2 )
				{
					( ( unsigned * ) ( dst_row + x1 ) )[ 0 ] = ( ( unsigned * ) palette )[ src_row[ x2 ] ];
				}
				break;
			}
		}

		if ( lpsFramebuffer->Unlock( ( LPRECT ) lpSurface ) != DD_OK )
			return "Unlocking surface";

		//if ( lpDD->WaitForVerticalBlank( DDWAITVB_BLOCKBEGIN, NULL ) != DD_OK )
		//	return "Waiting for vertical blank";
		if ( wait ) WaitForSingleObject( wait_event, 100 );

		rcSource.right -= rcSource.left;
		rcSource.bottom -= rcSource.top;
		rcSource.left = 0;
		rcSource.top = 0;

		rcLast = rcSource;

		HRESULT hRes = lpsPrimary->Blt( & rcWindow, lpsFramebuffer, & rcSource, DDBLT_ASYNC, 0 );
		if ( hRes == DDERR_SURFACELOST )
		{
			const char * err = restore_surfaces();
			if ( err ) return err;

			hRes = lpsPrimary->Blt( &rcWindow, lpsFramebuffer, &rcSource, DDBLT_ASYNC, 0 );
		}

		if ( hRes == DDERR_SURFACELOST )
			return "Surface lost";

		if ( hRes != DD_OK )
			return "Copying to primary surface";

		ValidateRect( hWnd, & rcWindow );

		return 0;
	}

	virtual void repaint()
	{
		if ( lpsPrimary->Blt( & rcWindow, lpsFramebuffer, & rcLast, DDBLT_ASYNC, 0 ) == DDERR_SURFACELOST )
			restore_surfaces();
	}

	virtual void clear()
	{
		DDBLTFX fx;
		ZeroMemory( & fx, sizeof( fx ) );
		fx.dwSize = sizeof( fx );
		//fx.dwFillColor = 0;

		if ( lpsFramebuffer->Blt( 0, 0, 0, DDBLT_COLORFILL | DDBLT_ASYNC, & fx ) == DDERR_SURFACELOST )
			restore_surfaces();
	}

private:
	const char * restore_surfaces()
	{
		if ( lpsPrimary->Restore() != DD_OK ||
		     lpsFramebuffer->Restore() != DD_OK )
			 return "Surface lost";

		return 0;
	}

	bool StartTimer()
	{
		bool        res = false;
		MMRESULT    result;

		TIMECAPS    tc;

		if ( TIMERR_NOERROR == timeGetDevCaps( & tc, sizeof( TIMECAPS ) ) )
		{
			wait_timerres = min( max( tc.wPeriodMin, 1 ), tc.wPeriodMax );
			timeBeginPeriod( wait_timerres );
		}
		else 
		{
			return false;
		}    

		result = timeSetEvent( wait_timerres, wait_timerres, & display_i_ddraw::g_timer_proc, ( DWORD_PTR ) this, TIME_PERIODIC );

		if (NULL != result)
		{
			wait_timerid = (UINT)result;
			res = true;
		}

		return res;
	}

	void StopTimer()
	{
		MMRESULT    result;

		result = timeKillEvent( wait_timerid );
		if ( TIMERR_NOERROR == result )
		{
			wait_timerid = 0;
		}


		if ( 0 != wait_timerres )
		{
			timeEndPeriod( wait_timerres );
			wait_timerres = 0;
		}
	}

	static void CALLBACK g_timer_proc( UINT id, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2 )
	{
		display_i_ddraw * p_this = reinterpret_cast< display_i_ddraw * >( dwUser );
		if ( p_this )
		{
			p_this->timer_proc( id, msg, dw1, dw2 );
		}
	}

public:
	void timer_proc( UINT id, UINT msg, DWORD_PTR dw1, DWORD_PTR dw2 )
	{
		DWORD scanline;
		if ( lpDD->GetScanLine( & scanline ) == DD_OK )
		{
			if ( scanline > wait_screenheight ) wait_screenheight = scanline;

			scanline = ( scanline + wait_screenheight - wait_firstline ) % wait_screenheight;

			if ( scanline < wait_lastscanline )
			{
				PulseEvent( wait_event );
			}

			wait_lastscanline = scanline;
		}
	}
};

display * create_display()
{
	return new display_i_ddraw;
}