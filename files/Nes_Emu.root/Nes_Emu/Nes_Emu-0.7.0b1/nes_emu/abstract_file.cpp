
#include "abstract_file.h"

#include "blargg_config.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* Copyright (C) 2005-2006 Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// to do: remove?
#ifndef RAISE_ERROR
	#define RAISE_ERROR( str ) return str
#endif

typedef Data_Reader::error_t error_t;

error_t Data_Writer::write( const void*, long ) { return 0; }

void Data_Writer::satisfy_lame_linker_() { }

error_t Data_Reader::read( void* p, long s )
{
	long result = read_avail( p, s );
	if ( result != s )
	{
		if ( result >= 0 && result < s )
			RAISE_ERROR( "Unexpected end-of-file" );
		
		RAISE_ERROR( "Read error" );
	}
	
	return 0;
}

error_t Data_Reader::skip( long count )
{
	char buf [512];
	while ( count )
	{
		int n = sizeof buf;
		if ( n > count )
			n = count;
		count -= n;
		error_t err = read( buf, n );
		if ( err )
			RAISE_ERROR( err );
	}
	return 0;
}

long File_Reader::remain() const
{
	return size() - tell();
}

error_t File_Reader::skip( long n )
{
	assert( n >= 0 );
	if ( n )
		RAISE_ERROR( seek( tell() + n ) );
	
	return 0;
}


// Subset_Reader

Subset_Reader::Subset_Reader( Data_Reader* in_, long size ) :
	in( in_ ),
	remain_( in_->remain() )
{
	if ( remain_ > size )
		remain_ = size;
}

long Subset_Reader::remain() const {
	return remain_;
}

long Subset_Reader::read_avail( void* p, long s )
{
	if ( s > remain_ )
		s = remain_;
	remain_ -= s;
	return in->read_avail( p, s );
}

// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	begin( (const char*) p ),
	pos( 0 ),
	size_( s )
{
}
	
long Mem_File_Reader::size() const {
	return size_;
}

long Mem_File_Reader::read_avail( void* p, long s )
{
	long r = remain();
	if ( s > r )
		s = r;
	memcpy( p, begin + pos, s );
	pos += s;
	return s;
}

long Mem_File_Reader::tell() const {
	return pos;
}

error_t Mem_File_Reader::seek( long n )
{
	if ( n > size_ )
		RAISE_ERROR( "Tried to go past end of file" );
	pos = n;
	return 0;
}

// Std_File_Reader

Std_File_Reader::Std_File_Reader() : file_( 0 ) {
}

Std_File_Reader::~Std_File_Reader() {
	close();
}

error_t Std_File_Reader::open( const char* path )
{
	close();
	file_ = fopen( path, "rb" );
	if ( !file_ )
		RAISE_ERROR( "Couldn't open file" );
	return 0;
}

long Std_File_Reader::size() const
{
	long pos = tell();
	fseek( file_, 0, SEEK_END );
	long result = tell();
	fseek( file_, pos, SEEK_SET );
	return result;
}

long Std_File_Reader::read_avail( void* p, long s ) {
	return (long) fread( p, 1, s, file_ );
}

long Std_File_Reader::tell() const {
	return ftell( file_ );
}

error_t Std_File_Reader::seek( long n )
{
	if ( fseek( file_, n, SEEK_SET ) != 0 )
		RAISE_ERROR( "Error seeking in file" );
	return 0;
}

void Std_File_Reader::close()
{
	if ( file_ ) {
		fclose( file_ );
		file_ = 0;
	}
}

// Std_File_Writer

Std_File_Writer::Std_File_Writer() : file_( 0 ) {
}

Std_File_Writer::~Std_File_Writer() {
	close();
}

error_t Std_File_Writer::open( const char* path )
{
	close();
	file_ = fopen( path, "wb" );
	if ( !file_ )
		RAISE_ERROR( "Couldn't open file for writing" );
		
	// to do: increase file buffer size
	//setvbuf( file_, 0, _IOFBF, 32 * 1024L );
	
	return 0;
}

error_t Std_File_Writer::write( const void* p, long s )
{
	long result = (long) fwrite( p, 1, s, file_ );
	if ( result != s )
		RAISE_ERROR( "Couldn't write to file" );
	return 0;
}

void Std_File_Writer::close()
{
	if ( file_ ) {
		fclose( file_ );
		file_ = 0;
	}
}

// Mem_Writer

Mem_Writer::Mem_Writer( void* p, long s, int b )
{
	data_ = (char*) p;
	size_ = 0;
	allocated = s;
	mode = b ? ignore_excess : fixed;
}

Mem_Writer::Mem_Writer()
{
	data_ = 0;
	size_ = 0;
	allocated = 0;
	mode = expanding;
}

Mem_Writer::~Mem_Writer()
{
	if ( mode == expanding )
		free( data_ );
}

error_t Mem_Writer::write( const void* p, long s )
{
	long remain = allocated - size_;
	if ( s > remain )
	{
		if ( mode == fixed )
			RAISE_ERROR( "Tried to write more data than expected" );
		
		if ( mode == ignore_excess )
		{
			s = remain;
		}
		else // expanding
		{
			long new_allocated = size_ + s;
			new_allocated += (new_allocated >> 1) + 2048;
			void* p = realloc( data_, new_allocated );
			if ( !p )
				RAISE_ERROR( "Out of memory" );
			data_ = (char*) p;
			allocated = new_allocated;
		}
	}
	
	assert( size_ + s <= allocated );
	memcpy( data_ + size_, p, s );
	size_ += s;
	
	return 0;
}

// Null_Writer

error_t Null_Writer::write( const void*, long )
{
	return 0;
}

// Auto_File_Reader

#ifndef STD_AUTO_FILE_WRITER
	#define STD_AUTO_FILE_WRITER Std_File_Writer
#endif

#ifdef HAVE_ZLIB_H
	#ifndef STD_AUTO_FILE_READER
		#define STD_AUTO_FILE_READER Gzip_File_Reader
	#endif

	#ifndef STD_AUTO_FILE_COMP_WRITER
		#define STD_AUTO_FILE_COMP_WRITER Gzip_File_Writer
	#endif

#else
	#ifndef STD_AUTO_FILE_READER
		#define STD_AUTO_FILE_READER Std_File_Reader
	#endif

	#ifndef STD_AUTO_FILE_COMP_WRITER
		#define STD_AUTO_FILE_COMP_WRITER Std_File_Writer
	#endif

#endif

const char* Auto_File_Reader::open()
{
	#ifdef DISABLE_AUTO_FILE
		return 0;
	#else
		if ( data )
			return 0;
		STD_AUTO_FILE_READER* d = new STD_AUTO_FILE_READER;
		if ( !d )
			RAISE_ERROR( "Out of memory" );
		data = d;
		return d->open( path );
	#endif
}

Auto_File_Reader::~Auto_File_Reader()
{
	if ( path )
		delete data;
}

// Auto_File_Writer

const char* Auto_File_Writer::open()
{
	#ifdef DISABLE_AUTO_FILE
		return 0;
	#else
		if ( data )
			return 0;
		STD_AUTO_FILE_WRITER* d = new STD_AUTO_FILE_WRITER;
		if ( !d )
			RAISE_ERROR( "Out of memory" );
		data = d;
		return d->open( path );
	#endif
}

const char* Auto_File_Writer::open_comp( int level )
{
	#ifdef DISABLE_AUTO_FILE
		return 0;
	#else
		if ( data )
			return 0;
		STD_AUTO_FILE_COMP_WRITER* d = new STD_AUTO_FILE_COMP_WRITER;
		if ( !d )
			RAISE_ERROR( "Out of memory" );
		data = d;
		return d->open( path, level );
	#endif
}

Auto_File_Writer::~Auto_File_Writer()
{
	#ifndef DISABLE_AUTO_FILE
		if ( path )
			delete data;
	#endif
}

#ifdef HAVE_ZLIB_H

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

const char* get_gzip_eof( const char* path, long* eof )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return "Couldn't open file";
	const char* error = get_gzip_eof( file, eof );
	fclose( file );
	return error;
}

// Gzip_File_Reader

Gzip_File_Reader::Gzip_File_Reader() : file_( 0 )
{
}

Gzip_File_Reader::~Gzip_File_Reader()
{
	close();
}

Gzip_File_Reader::error_t Gzip_File_Reader::open( const char* path )
{
	close();
	
	error_t error = get_gzip_eof( path, &size_ );
	if ( error )
		RAISE_ERROR( error );
	
	file_ = gzopen( path, "rb" );
	if ( !file_ )
		RAISE_ERROR( "Couldn't open file" );
	
	return 0;
}

long Gzip_File_Reader::size() const
{
	return size_;
}

long Gzip_File_Reader::read_avail( void* p, long s )
{
	return (long) gzread( file_, p, s );
}

long Gzip_File_Reader::tell() const
{
	return gztell( file_ );
}

Gzip_File_Reader::error_t Gzip_File_Reader::seek( long n )
{
	if ( gzseek( file_, n, SEEK_SET ) < 0 )
		RAISE_ERROR( "Error seeking in file" );
	return 0;
}

void Gzip_File_Reader::close()
{
	if ( file_ )
	{
		gzclose( file_ );
		file_ = 0;
	}
}

// Gzip_File_Writer

Gzip_File_Writer::Gzip_File_Writer() : file_( 0 )
{
}

Gzip_File_Writer::~Gzip_File_Writer()
{
	close();
}

Gzip_File_Writer::error_t Gzip_File_Writer::open( const char* path, int level )
{
	close();
	
	char mode [4] = { 'w', 'b', 0, 0 };
	if ( level >= 0 )
		mode [2] = level + '0';
	file_ = gzopen( path, mode );
	if ( !file_ )
		return "Couldn't open file for writing";
	
	return 0;
}

Gzip_File_Writer::error_t Gzip_File_Writer::write( const void* p, long s )
{
	long result = (long) gzwrite( file_ , (void*) p, s );
	if ( result != s )
		return "Couldn't write to file";
	return 0;
}

void Gzip_File_Writer::close()
{
	if ( file_ )
	{
		gzclose( file_ );
		file_ = 0;
	}
}

#endif
