#ifndef _CORE_CONFIG_H_
#define _CORE_CONFIG_H_

#include <tchar.h>

#include <boost/static_assert.hpp>

struct core_config_t
{
	typedef unsigned char var;

	var record_indefinitely;

	var show_status_bar;

	core_config_t();
};

BOOST_STATIC_ASSERT( sizeof ( core_config_t ) == 2 );

void core_config_load( core_config_t & config, const TCHAR * save_path );
void core_config_save( const core_config_t & config, const TCHAR * save_path );


struct sound_config_t
{
	typedef unsigned char var;

	unsigned short sample_rate;


	var effects_enabled;

	var treble;
	var bass;
	var echo_depth;

	sound_config_t();
};

BOOST_STATIC_ASSERT( sizeof ( sound_config_t ) == 6 );

void sound_config_load( sound_config_t & config, const TCHAR * save_path );
void sound_config_save( const sound_config_t & config, const TCHAR * save_path );

#endif