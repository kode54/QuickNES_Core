#include "../SDK/foobar2000.h"

#include "ds_stream.h"

#include <commctrl.h>
#include <dsound.h>

#include "resource.h"

class output_dsound;

static ds_api * g_api = 0;
static bool g_shutdown = false;
static critical_section apisync;
static ptr_list_t<output_dsound> g_instances;

static cfg_guid cfg_dsound_device("output_device");
static cfg_int cfg_allow_hwmix("allow hardware mixing",1);
static GUID null_guid;
static cfg_int cfg_dsound_bufsize("dsound_buffer_size",1000);
static cfg_int cfg_fade_len("fade_len",200);
static cfg_int cfg_fade_wait("fade_wait",0);

static double get_fade_len() {return (double)(int)cfg_fade_len / 1000.0;}

class output_dsound : public output_i_base
{
	ds_stream * p_stream;
	bool use_fadein;
public:

	virtual int open_ex(int srate,int bps,int nch,int format_code)
	{
		insync(apisync);
		if (p_stream)
		{
			p_stream->release();
			p_stream = 0;
		}
		
		

		bool b_float;

		if (format_code==DATA_FORMAT_LINEAR_PCM) b_float = false;
		else if (format_code==DATA_FORMAT_IEEE_FLOAT) b_float = true;
		else return 0;

		{
			if (g_shutdown) return 0;
			if (g_api==0) g_api = ds_api_create(core_api::get_main_window());
			if (g_api)
			{
				g_api->set_device(((GUID)cfg_dsound_device==null_guid) ? (const GUID*)0 : &(GUID)cfg_dsound_device);

				ds_stream_config cfg;
				cfg.srate = srate;
				cfg.nch = nch;
				cfg.bps = bps;
				cfg.b_float = b_float;
				cfg.b_allow_hwmix = !!cfg_allow_hwmix;
				cfg.buffer_ms = cfg_dsound_bufsize;
				p_stream = g_api->ds_stream_create(&cfg);
			}
		}

		if (p_stream==0) return 0;

		console::info(uStringPrintf("Created stream: %uHz %ubps %uch",srate,bps,nch));

		if (use_fadein)
		{
			p_stream->fade(0,1,get_fade_len());
			use_fadein = false;
		}
		return 1;
	}

	virtual int can_write()
	{
		insync(apisync);
		return p_stream ? (int)p_stream->can_write_bytes() : 0;
	}
	virtual int write(const char * data,int bytes)
	{
		insync(apisync);
		return p_stream ? !!p_stream->write(data,bytes) : 0;
	}
	
	virtual int is_playing()
	{
		insync(apisync);
		return p_stream ? !!p_stream->is_playing() : 0;
	}


	virtual GUID get_guid()
	{
		// {D321A682-42A9-44c3-857E-071A3F5D799E}
		static const GUID guid = 
		{ 0xd321a682, 0x42a9, 0x44c3, { 0x85, 0x7e, 0x7, 0x1a, 0x3f, 0x5d, 0x79, 0x9e } };
		return guid;
	}

	output_dsound()
	{
		insync(apisync);
		g_instances.add_item(this);
		use_fadein = false;
		p_stream = 0;
	}

	~output_dsound()
	{
		insync(apisync);
		g_instances.remove_item(this);
		if (p_stream) p_stream->fade_and_forget(get_fade_len());
	}

	virtual int do_flush()
	{
		insync(apisync);
		if (p_stream)
		{
			p_stream->fade_and_forget(get_fade_len());
			p_stream = 0;
			use_fadein = true;
		}
		return 1;
	}

	virtual void pause(int state)
	{
		insync(apisync);
		if (p_stream) p_stream->fade_pause(get_fade_len(),!!state);
	}

	virtual int get_latency_bytes()
	{
		insync(apisync);
		return p_stream ? p_stream->get_latency_bytes() : 0;
	}

	virtual void force_play()
	{
		insync(apisync);
		if (p_stream) p_stream->force_play();
	}

	virtual const char * get_name()
	{
		return "DirectSound v2.0";
	}

	virtual const char * get_config_page_name() {return get_name();}

	void on_shutdown()
	{
		if (p_stream)
		{
			p_stream->release();
			p_stream = 0;
		}
	}
};


static service_factory_t<output,output_dsound> foo;

DECLARE_COMPONENT_VERSION("DirectSound output","2.0",0);

static void shutdown(bool force)
{
	insync(apisync);
	g_shutdown = true;
	{
		unsigned n;
		for(n=0;n<g_instances.get_count();n++)
			g_instances[n]->on_shutdown();
	}
	if (g_api)
	{
		g_api->release(!force && cfg_fade_wait);
		g_api = 0;
	}
}

class myinitquit : public initquit
{
public:
	void on_init() {}
	void on_quit()
	{
		shutdown(false);
	}
	void on_system_shutdown() 
	{
		shutdown(true);
	}
};

static service_factory_single_t<initquit,myinitquit> asdfasdf;

static void get_device_info(GUID guid,string8 & out)
{
	bool success = false;
	DSCAPS caps;
	memset(&caps,0,sizeof(caps));
	caps.dwSize = sizeof(caps);
	{
		IDirectSound * pDS;
		if (SUCCEEDED(DirectSoundCreate(&guid,&pDS,0)))
		{
			if (SUCCEEDED(pDS->GetCaps(&caps)))
			{
				success = true;
			}
			pDS->Release();
		}
	}

	if (success)
	{
		out.reset();
		if (caps.dwFlags & DSCAPS_EMULDRIVER) out += "Driver is emulated.\n";
		if (caps.dwMaxHwMixingAllBuffers>1)
		{
			out += "Hardware mixing available, ";
			out.add_uint(caps.dwMaxHwMixingAllBuffers);
			out += " streams total (";
			out.add_uint(caps.dwFreeHwMixingAllBuffers);
			out += " free)\n";
		}
		else out += "Hardware mixing not available.\n";
		if (caps.dwTotalHwMemBytes>0)
		{
			out += "Onboard memory: ";
			out.add_uint(caps.dwTotalHwMemBytes);
			out += " bytes.\n";
		}
		else out += "Onboard memory not available.\n";
	}
	else
	{
		out = "Error getting device info.\n";
	}
}


static BOOL CALLBACK enum_callback(
  const GUID * lpGuid,
  const void * lpcstrDescription,
  const void * lpcstrModule,
  HWND list
)
{
	int idx = uSendMessage(list,CB_ADDSTRING,0,(long)lpcstrDescription);//avoid going through utf8
	uSendMessage(list,CB_SETITEMDATA,idx,(long)lpGuid);
	if ((lpGuid==0 && cfg_dsound_device==null_guid) || (lpGuid!=0 && cfg_dsound_device==*lpGuid))
		uSendMessage(list,CB_SETCURSEL,idx,0);
	return 1;
}
 
static void update_bufsize(HWND wnd)
{
	int val = cfg_dsound_bufsize;
	char msg[64];
	sprintf(msg,"%u ms",val);
	uSetDlgItemText(wnd,IDC_BUFSIZE_DISPLAY,msg);
}

static BOOL CALLBACK ConfigProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	enum {MIN_BUFFER = 100/10};
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			uSendDlgItemMessage(wnd,IDC_ALLOW_HWMIX,BM_SETCHECK,cfg_allow_hwmix,0);

			HWND list = GetDlgItem(wnd,IDC_DEVICE);
			if (IsUnicode())
			{
				HRESULT (WINAPI *pDirectSoundEnumerateW)(
				  LPDSENUMCALLBACKW lpDSEnumCallback,  
				  LPVOID lpContext                    
				);
				pDirectSoundEnumerateW = 
				(HRESULT (WINAPI *)(LPDSENUMCALLBACKW lpDSEnumCallback,  LPVOID lpContext ) )
					GetProcAddress((HMODULE)GetModuleHandle("dsound.dll"),"DirectSoundEnumerateW"
					);


				if (pDirectSoundEnumerateW)
					pDirectSoundEnumerateW((LPDSENUMCALLBACKW)enum_callback,(void*)list);
			}
			else
			{
				DirectSoundEnumerateA((LPDSENUMCALLBACKA)enum_callback,(void*)list);
			}

			if (uSendMessage(list,CB_GETCURSEL,0,0)<0)
			{
				uSendMessage(list,CB_SETCURSEL,0,0);
				cfg_dsound_device=null_guid;
			}

			uSendDlgItemMessage(wnd,IDC_BUFSIZE,TBM_SETRANGE,0,MAKELONG(MIN_BUFFER,800));
			uSendDlgItemMessage(wnd,IDC_BUFSIZE,TBM_SETPOS,1,cfg_dsound_bufsize/10);
			
			update_bufsize(wnd);

			{
				string8 temp;
				get_device_info(cfg_dsound_device,temp);
				uSetDlgItemText(wnd,IDC_DEV_INFO,temp);
			}

			uSendDlgItemMessage(wnd,IDC_FADELEN_SPIN,UDM_SETRANGE,0,MAKELONG(10000,0));
			uSetDlgItemInt(wnd,IDC_FADELEN,cfg_fade_len,0);
			uSendDlgItemMessage(wnd,IDC_FULLFADEOUT,BM_SETCHECK,cfg_fade_wait,0);
		}
		break;
	case WM_HSCROLL:
		cfg_dsound_bufsize = uSendDlgItemMessage(wnd,IDC_BUFSIZE,TBM_GETPOS,0,0)*10;
		update_bufsize(wnd);
		break;
	case WM_DESTROY:
		cfg_fade_len = uGetDlgItemInt(wnd,IDC_FADELEN,0,0);
		break;
	case WM_COMMAND:
		switch(wp)
		{
		case IDC_DEV_INFO_REFRESH:
			{
				string8 temp;
				get_device_info(cfg_dsound_device,temp);
				uSetDlgItemText(wnd,IDC_DEV_INFO,temp);
			}
			break;
		case IDC_ALLOW_HWMIX:
			cfg_allow_hwmix = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
			break;
		case (CBN_SELCHANGE<<16)|IDC_DEVICE:
			{
				int idx = uSendMessage((HWND)lp,CB_GETCURSEL,0,0);
				if (idx>=0)
				{
					GUID * g = (GUID*)uSendMessage((HWND)lp,CB_GETITEMDATA,idx,0);
					if (g) cfg_dsound_device = *g;
					else cfg_dsound_device=null_guid;
					{
						string8 temp;
						get_device_info(cfg_dsound_device,temp);
						uSetDlgItemText(wnd,IDC_DEV_INFO,temp);
					}

				}
			}
			break;
		case IDC_FULLFADEOUT:
			cfg_fade_wait = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
			break;
		}
		break;
	}
	return 0;
}

class config_dsound : public config
{
public:
	HWND create(HWND parent)
	{
		return uCreateDialog(IDD_CONFIG_DSOUND,parent,ConfigProc);
	}
	const char * get_name() {return "DirectSound v2.0";}
	const char * get_parent_name() {return "Output";}
};


static service_factory_single_t<config,config_dsound> foo2;
