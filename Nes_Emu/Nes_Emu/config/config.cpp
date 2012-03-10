#include "stdafx.h"

#include "config.h"

#include "../std_file_u.h"

#include <nes_emu/blargg_endian.h>

#include <string>

#include <windows.h>

namespace std
{
#ifdef UNICODE
	typedef wstring tstring;
#else
	typedef string tstring;
#endif
}

core_config_t::core_config_t()
{
	record_indefinitely = 0;
	show_status_bar = 1;
}

static const GUID core_signature = { 0x904350ee, 0x9a6a, 0x4db2, { 0xad, 0x48, 0xf6, 0xec, 0xd4, 0x94, 0x78, 0xcd } };

void core_config_load( core_config_t & config, const TCHAR * save_path )
{
	GUID temp;
	Std_File_Reader_u in;
	const char * err = in.open( ( std::tstring( save_path ) + _T( "core.cfg" ) ).c_str() );
	if ( ! err ) err = in.read( & temp, sizeof( temp ) );
	if ( ! err && temp != core_signature ) err = "Invalid signature";
	if ( ! err ) err = in.read( & config, sizeof( config ) );
}

void core_config_save( const core_config_t & config, const TCHAR * save_path )
{
	Std_File_Writer_u out;
	const char * err = out.open( ( std::tstring( save_path ) + _T( "core.cfg" ) ).c_str() );
	if ( ! err ) err = out.write( & core_signature, sizeof( core_signature ) );
	if ( ! err ) err = out.write( & config, sizeof( config ) );
}


sound_config_t::sound_config_t()
{
	sample_rate = 44100;

	effects_enabled = 0;

	treble = 128;
	bass = 128;
	echo_depth = 31;
}

static const GUID sound_signature = { 0xa45c67c5, 0x114d, 0x4ae6, { 0xa9, 0x77, 0x7b, 0x2e, 0x67, 0x33, 0xc2, 0xde } };

void sound_config_load( sound_config_t & config, const TCHAR * save_path )
{
	GUID temp;
	Std_File_Reader_u in;
	const char * err = in.open( ( std::tstring( save_path ) + _T( "sound.cfg" ) ).c_str() );
	if ( ! err ) err = in.read( & temp, sizeof( temp ) );
	if ( ! err && temp != sound_signature ) err = "Invalid signature";
	if ( ! err ) err = in.read( & config, sizeof( config ) );
	if ( ! err ) config.sample_rate = get_le16( & config.sample_rate );
	if ( config.sample_rate < 6000 ) config.sample_rate = 6000;
	else if ( config.sample_rate > 48000 ) config.sample_rate = 48000;
}

void sound_config_save( const sound_config_t & config, const TCHAR * save_path )
{
	Std_File_Writer_u out;
	const char * err = out.open( ( std::tstring( save_path ) + _T( "sound.cfg" ) ).c_str() );
	if ( ! err ) err = out.write( & sound_signature, sizeof( sound_signature ) );
	if ( ! err )
	{
		sound_config_t temp = config;
		set_le16( &temp.sample_rate, temp.sample_rate );
		err = out.write( & temp, sizeof( temp ) );
	}
}

display_config_t::display_config_t()
{
	vsync = 1;
}

static const GUID display_signature = { 0x4ac1870, 0xd775, 0x4abf, { 0x85, 0xcb, 0x47, 0x3e, 0x77, 0xe4, 0xf5, 0xd2 } };

void display_config_load( display_config_t & config, const TCHAR * save_path )
{
	GUID temp;
	Std_File_Reader_u in;
	const char * err = in.open( ( std::tstring( save_path ) + _T( "display.cfg" ) ).c_str() );
	if ( ! err ) err = in.read( & temp, sizeof( temp ) );
	if ( ! err && temp != display_signature ) err = "Invalid signature";
	if ( ! err ) err = in.read( & config, sizeof( config ) );
}

void display_config_save( const display_config_t & config, const TCHAR * save_path )
{
	Std_File_Writer_u out;
	const char * err = out.open( ( std::tstring( save_path ) + _T( "display.cfg" ) ).c_str() );
	if ( ! err ) err = out.write( & display_signature, sizeof( display_signature ) );
	if ( ! err ) err = out.write( & config, sizeof( config ) );
}
