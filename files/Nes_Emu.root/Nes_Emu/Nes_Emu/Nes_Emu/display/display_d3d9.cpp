#include "stdafx.h"
#include "display.h"

#include <windows.h>
#include <commctrl.h>
#include <tchar.h>

#define DIRECT3D_VERSION 0x0900
#include <d3d9.h>

#pragma comment( lib, "d3d9.lib" )

unsigned rounded_power_of_two(unsigned n)
{
	n--;
	n |= n >>  1;
	n |= n >>  2;
	n |= n >>  4;
	n |= n >>  8;
	n |= n >> 16;
	return n + 1;
}

class display_i_d3d9 : public display
{
	HWND                    hWnd;

	RECT                    rcWindow;
	RECT                    rcLast;

	LPDIRECT3D9             lpd3d;
	LPDIRECT3DDEVICE9       lpdev;
	LPDIRECT3DVERTEXBUFFER9 lpvbuf, *vertex_ptr;
	LPDIRECT3DTEXTURE9      lptex;
	LPDIRECT3DSURFACE9      lpsurface;
	D3DCAPS9                d3dcaps;
	D3DSURFACE_DESC         d3dsd;
	D3DLOCKED_RECT          d3dlr;
	D3DPRESENT_PARAMETERS   dpp;

	unsigned                input_width, input_height;
	unsigned                surface_width, surface_height;

	struct d3dvertex
	{
		float x, y, z, rhw;  //screen coords
		float u, v;          //texture coords
	};

	struct
	{
		unsigned t_usage, v_usage;
		unsigned t_pool,  v_pool;
		unsigned lock;
	} flags;

	bool lost;

public:
	display_i_d3d9()
	{
		hWnd = 0;

		lpd3d = 0;
		lpdev = 0;
		lpvbuf = 0;
		lptex = 0;
		lpsurface = 0;

		lost = false;
	}

	virtual ~display_i_d3d9()
	{
		release_objects();

		if ( lpdev )
		{
			lpdev->Release();
			lpdev = 0;
		}

		if ( lpd3d )
		{
			lpd3d->Release();
			lpd3d = 0;
		}
	}

	virtual const char* open( unsigned buffer_width, unsigned buffer_height, HWND hWnd )
	{
		if ( this->hWnd )
			return "Display already open";

		this->hWnd = hWnd;

		lpd3d = Direct3DCreate9( D3D_SDK_VERSION );
		if ( ! lpd3d )
			return "Creating Direct3D 9 object";

		D3DDISPLAYMODE mode;
		lpd3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, & mode );

		ZeroMemory(&dpp, sizeof(dpp));
		dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
		dpp.SwapEffect             = D3DSWAPEFFECT_FLIP;
		dpp.hDeviceWindow          = hWnd;
		dpp.BackBufferCount        = 1;
		dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
		dpp.MultiSampleQuality     = 0;
		dpp.EnableAutoDepthStencil = false;
		dpp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
		dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;
		dpp.Windowed               = TRUE;
		dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
		dpp.BackBufferWidth        = 640;
		dpp.BackBufferHeight       = 480;

		if ( lpd3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, & dpp, & lpdev ) != D3D_OK )
			return "Creating Direct3D 9 device object";

		lpdev->GetDeviceCaps(&d3dcaps);

		bool dynamic = !!(d3dcaps.Caps2 & D3DCAPS2_DYNAMICTEXTURES);

		if(dynamic)
		{
			flags.t_usage = D3DUSAGE_DYNAMIC;
			flags.v_usage = D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC;
			flags.t_pool  = D3DPOOL_DEFAULT;
			flags.v_pool  = D3DPOOL_DEFAULT;
			flags.lock    = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD;
		}
		else
		{
			flags.t_usage = 0;
			flags.v_usage = D3DUSAGE_WRITEONLY;
			flags.t_pool  = D3DPOOL_MANAGED;
			flags.v_pool  = D3DPOOL_MANAGED;
			flags.lock    = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD;
		}

		input_width = buffer_width;
		input_height = buffer_height;

		surface_width = rounded_power_of_two( buffer_width );
		surface_height = rounded_power_of_two( buffer_height );

		if ( surface_width > d3dcaps.MaxTextureWidth || surface_height > d3dcaps.MaxTextureHeight )
			return "Device texture size limit is too small";

		restore_objects();

		update_position();

		clear();

		return 0;
	}

	virtual void update_position()
	{
		GetClientRect( hWnd, & rcWindow );

		unsigned width = rcWindow.right - rcWindow.left;
		unsigned height = rcWindow.bottom - rcWindow.top;

		if ( width && height )
		{
			if ( dpp.BackBufferWidth != width || dpp.BackBufferHeight != height )
			{
				dpp.BackBufferWidth = width;
				dpp.BackBufferHeight = height;
				lost = true;
				restore_objects();
				update_filtering( 1 );
			}
		}
	}

	virtual const char* lock_framebuffer( void *& buffer, unsigned & pitch )
	{
		if ( lost && !restore_objects() ) return "Lock failed";

		lptex->GetLevelDesc(0, &d3dsd);
		if ( lptex->GetSurfaceLevel(0, &lpsurface) != D3D_OK )
			return "Lock failed";

		if ( lpsurface->LockRect(&d3dlr, 0, flags.lock) != D3D_OK )
			return "Lock failed";
		buffer = d3dlr.pBits;
		pitch = d3dlr.Pitch;

		return buffer != 0 ? 0 : "Lock failed";
	}

	virtual const char* unlock_framebuffer()
	{
		lpsurface->UnlockRect();
		lpsurface->Release();
		lpsurface = 0;

		return 0;
	}

	virtual const char* paint( bool wait )
	{
		if ( dpp.PresentationInterval != ( wait ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE ) )
		{
			dpp.PresentationInterval = wait ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
			lost = true;
			restore_objects();
			update_filtering( 1 );
		}

		if ( lost && !restore_objects() ) return "Surface lost";

		repaint();

		return 0;
	}

	virtual void repaint()
	{
		if ( lost && restore_objects() ) return;

		lpdev->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x000000ff, 1.0f, 0L );

		HRESULT rv = lpdev->BeginScene();

		if( SUCCEEDED( rv ) )
		{
			set_vertex( 0, 0, input_width, input_height, surface_width, surface_height, 0, 0, rcWindow.right, rcWindow.bottom );
			lpdev->SetTexture( 0, lptex );
			lpdev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
			lpdev->EndScene();

			if ( lpdev->Present( NULL, NULL, NULL, NULL ) == D3DERR_DEVICELOST ) lost = true;
		}
		else lost = true;

		ValidateRect( hWnd, & rcWindow );
	}

	virtual void clear()
	{
		if ( lost && !restore_objects() ) return;

		lptex->GetLevelDesc(0, &d3dsd);
		lptex->GetSurfaceLevel(0, &lpsurface);

		if ( lpsurface )
		{
			lpdev->ColorFill( lpsurface, NULL, D3DCOLOR_XRGB(0x00, 0x00, 0x00) );
			lpsurface->Release();
			lpsurface = 0;
		}

		for ( unsigned i = 0; i < 3; i++ )
		{
			lpdev->Clear( 0, 0, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0x00, 0x00, 0x00), 1.0f, 0 );
			lpdev->Present( 0, 0, 0, 0 );
		}
	}

private:
	void set_vertex(
		unsigned px, unsigned py, unsigned pw, unsigned ph,
		unsigned tw, unsigned th,
		unsigned x,  unsigned y,  unsigned w,  unsigned h
	)
	{
		d3dvertex vertex[4];
		vertex[0].x = vertex[2].x = (double)(x     - 0.5);
		vertex[1].x = vertex[3].x = (double)(x + w - 0.5);
		vertex[0].y = vertex[1].y = (double)(y     - 0.5);
		vertex[2].y = vertex[3].y = (double)(y + h - 0.5);

		vertex[0].z = vertex[1].z = vertex[2].z = vertex[3].z = 0.0;
		vertex[0].rhw = vertex[1].rhw = vertex[2].rhw = vertex[3].rhw = 1.0;

		double rw = (double)w / (double)pw * (double)tw;
		double rh = (double)h / (double)ph * (double)th;
		vertex[0].u = vertex[2].u = (double)(px    ) / rw;
		vertex[1].u = vertex[3].u = (double)(px + w) / rw;
		vertex[0].v = vertex[1].v = (double)(py    ) / rh;
		vertex[2].v = vertex[3].v = (double)(py + h) / rh;

		lpvbuf->Lock( 0, sizeof( d3dvertex ) * 4, ( void ** ) &vertex_ptr, 0 );
		memcpy( vertex_ptr, vertex, sizeof( d3dvertex ) * 4 );
		lpvbuf->Unlock();

		lpdev->SetStreamSource( 0, lpvbuf, 0, sizeof( d3dvertex ) );
	}

	void release_objects()
	{
		if ( lpsurface )
		{
			lpsurface->Release();
			lpsurface = 0;
		}

		if ( lptex )
		{
			lptex->Release();
			lptex = 0;
		}

		if ( lpvbuf )
		{
			lpvbuf->Release();
			lpvbuf = 0;
		}
	}

	bool restore_objects()
	{
		if ( lost )
		{
			release_objects();
			if ( lpdev->Reset( &dpp ) != D3D_OK ) return false;
		}

		lost = false;

		LPDIRECT3DSURFACE9 lpbb;
		HRESULT hr;
		if( SUCCEEDED( hr = lpdev->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, & lpbb ) ) ) {
			lpbb->GetDesc( & d3dsd );
			lpbb->Release();
		}

		lpdev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
		lpdev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		lpdev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

		lpdev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
		lpdev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		lpdev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		lpdev->SetRenderState(D3DRS_LIGHTING, false);
		lpdev->SetRenderState(D3DRS_ZENABLE,  false);
		lpdev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		lpdev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		lpdev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		lpdev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);

		lpdev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

		if ( lpdev->CreateVertexBuffer( sizeof(d3dvertex) * 4, flags.v_usage, D3DFVF_XYZRHW | D3DFVF_TEX1, (D3DPOOL)flags.v_pool, &lpvbuf, NULL ) != D3D_OK )
			return false;

		update_filtering( 1 );

		lpdev->SetRenderState( D3DRS_DITHERENABLE,   TRUE );

		if ( lpdev->CreateTexture( surface_width, surface_height, 1, flags.t_usage, D3DFMT_X8R8G8B8, (D3DPOOL)flags.t_pool, &lptex, NULL ) != D3D_OK )
			return false;

		return true;
	}

	void update_filtering( int filter )
	{
		switch(filter)
		{
		default:
		case 0:
			lpdev->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
			lpdev->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
			lpdev->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
			break;

		case 1:
			lpdev->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
			lpdev->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
			lpdev->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
			break;
		}
	}
};

display * create_display_d3d9()
{
	return new display_i_d3d9;
}