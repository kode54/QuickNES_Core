#include "stdafx.h"

#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef RAISE_ERROR
	#define RAISE_ERROR( str ) return str
#endif

typedef blargg_err_t error_t;

blargg_err_t Std_File_Reader_u::open( const TCHAR path [] )
{
#ifdef _UNICODE
	char * path8 = blargg_to_utf8( path );
	blargg_err_t err = Std_File_Reader::open( path8 );
	free( path8 );
	return err;
#else
	return Std_File_Reader::open( path );
#endif
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

// Gzip_File_Reader_u

blargg_err_t Gzip_File_Reader_u::open( const TCHAR* path )
{
#ifdef _UNICODE
	char * path8 = blargg_to_utf8( path );
	blargg_err_t err = Gzip_File_Reader::open( path8 );
	free( path8 );
	return err;
#else
	return Gzip_File_Reader::open( path );
#endif
}

#include "zlib.h"

// Gzip_File_Writer

Gzip_File_Writer_u::Gzip_File_Writer_u() : file_( 0 )
{
}

Gzip_File_Writer_u::~Gzip_File_Writer_u()
{
	close();
}

Gzip_File_Writer_u::error_t Gzip_File_Writer_u::open( const TCHAR* path, int level )
{
	close();

	const int open_flags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY;
	const int open_permissions = _S_IREAD | _S_IWRITE;
//	int fd = _topen( path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE );
	// XXX BLAH zlib1 needs this handle to be from msvcrt.dll
	HMODULE hcrt = GetModuleHandle( _T("msvcrt.dll") );
	typedef int (__cdecl * p_close)(int);
	p_close func_close = (p_close) GetProcAddress( hcrt, "_close" );
#ifdef _UNICODE
	typedef int (__cdecl * p_wopen)(const wchar_t *, int, int);
	p_wopen func_wopen = (p_wopen) GetProcAddress( hcrt, "_wopen" );
	int fd = func_wopen( path, open_flags, open_permissions );
#else
	typedef int (__cdecl * p_open)(const char *, int, int);
	p_open func_open = (p_open) GetProcAddress( hcrt, "_open" );
	int fd = func_open( path, open_flags, open_permissions );
#endif
	if ( fd == -1 )
		return "Couldn't open file for writing";
	
	char mode [4] = { 'w', 'b', 0, 0 };
	if ( level >= 0 )
		mode [2] = level + '0';
	file_ = gzdopen( fd, mode );
	if ( !file_ )
	{
		func_close( fd );
		return "Couldn't open file for writing";
	}
	
	return 0;
}

Gzip_File_Writer_u::error_t Gzip_File_Writer_u::write( const void* p, long s )
{
	long result = (long) gzwrite( file_ , p, s );
	if ( result != s )
		return "Couldn't write to file";
	return 0;
}

void Gzip_File_Writer_u::close()
{
	if ( file_ )
	{
		gzclose( file_ );
		file_ = 0;
	}
}
