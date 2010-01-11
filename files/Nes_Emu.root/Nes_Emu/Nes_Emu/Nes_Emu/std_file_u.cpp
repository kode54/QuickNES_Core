#include "stdafx.h"

#include <stdio.h>

#ifndef RAISE_ERROR
	#define RAISE_ERROR( str ) return str
#endif

typedef Data_Reader::error_t error_t;

error_t Std_File_Reader_u::open( const TCHAR* path )
{
	reset( _tfopen( path, _T( "rb" ) ) );
	if ( ! file() )
		RAISE_ERROR( "Couldn't open file" );
	return NULL;
}

error_t Std_File_Writer_u::open( const TCHAR* path )
{
	reset( _tfopen( path, _T( "wb" ) ) );
	if ( ! file() )
		RAISE_ERROR( "Couldn't open file for writing" );

	// to do: increase file buffer size
	//setvbuf( file_, NULL, _IOFBF, 32 * 1024L );

	return NULL;
}

