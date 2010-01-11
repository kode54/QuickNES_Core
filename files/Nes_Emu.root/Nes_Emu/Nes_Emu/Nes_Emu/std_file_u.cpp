#include "stdafx.h"

#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

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

#include "zlib.h"

static const char* get_gzip_eof( FILE* file, long* eof )
{
	unsigned char buf [4];
	if ( !fread( buf, 2, 1, file ) )
		RAISE_ERROR( "Couldn't read from file" );
	
	if ( buf [0] == 0x1F && buf [1] == 0x8B )
	{
		if ( fseek( file, -4, SEEK_END ) )
			RAISE_ERROR( "Couldn't seek in file" );
		
		if ( !fread( buf, 4, 1, file ) )
			RAISE_ERROR( "Couldn't read from file" );
		
		*eof = buf [3] * 0x1000000L + buf [2] * 0x10000L + buf [1] * 0x100L + buf [0];
	}
	else
	{
		if ( fseek( file, 0, SEEK_END ) )
			RAISE_ERROR( "Couldn't seek in file" );
		
		*eof = ftell( file );
	}
	
	return 0;
}

const char* get_gzip_eof( const TCHAR* path, long* eof )
{
	FILE* file = _tfopen( path, _T("rb") );
	if ( !file )
		return "Couldn't open file";
	const char* error = get_gzip_eof( file, eof );
	fclose( file );
	return error;
}

// Gzip_File_Reader_u

Gzip_File_Reader_u::Gzip_File_Reader_u() : file_( 0 )
{
}

Gzip_File_Reader_u::~Gzip_File_Reader_u()
{
	close();
}

Gzip_File_Reader_u::error_t Gzip_File_Reader_u::open( const TCHAR* path )
{
	close();
	
	error_t error = get_gzip_eof( path, &size_ );
	if ( error )
		RAISE_ERROR( error );
	
	int fd = _topen( path, _O_RDONLY | _O_BINARY );
	if ( fd == -1 )
		RAISE_ERROR( "Couldn't open file" );
	file_ = gzdopen( fd, "rb" );
	if ( !file_ )
	{
		_close( fd );
		RAISE_ERROR( "Couldn't open file" );
	}
	
	return 0;
}

long Gzip_File_Reader_u::size() const
{
	return size_;
}

long Gzip_File_Reader_u::read_avail( void* p, long s )
{
	return (long) gzread( file_, p, s );
}

long Gzip_File_Reader_u::tell() const
{
	return gztell( file_ );
}

Gzip_File_Reader_u::error_t Gzip_File_Reader_u::seek( long n )
{
	if ( gzseek( file_, n, SEEK_SET ) < 0 )
		RAISE_ERROR( "Error seeking in file" );
	return 0;
}

void Gzip_File_Reader_u::close()
{
	if ( file_ )
	{
		gzclose( file_ );
		file_ = 0;
	}
}

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

	int fd = _topen( path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE );
	if ( fd == -1 )
		return "Couldn't open file for writing";
	
	char mode [4] = { 'w', 'b', 0, 0 };
	if ( level >= 0 )
		mode [2] = level + '0';
	file_ = gzdopen( fd, mode );
	if ( !file_ )
	{
		_close( fd );
		return "Couldn't open file for writing";
	}
	
	return 0;
}

Gzip_File_Writer_u::error_t Gzip_File_Writer_u::write( const void* p, long s )
{
	long result = (long) gzwrite( file_ , (void*) p, s );
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
