#ifndef _STD_FILE_H_
#define _STD_FILE_H_

#include <nes_emu/abstract_file.h>
#include <tchar.h>

class Std_File_Reader_u : public Std_File_Reader
{
public:
	blargg_err_t open( const TCHAR* path );
};

class Std_File_Writer_u : public Std_File_Writer
{
public:
	blargg_err_t open( const TCHAR* path );
};

class Gzip_File_Reader_u : public Gzip_File_Reader
{
public:
	blargg_err_t open( const TCHAR* );
};

class Gzip_File_Writer_u : public Data_Writer
{
	void* file_;
public:
	Gzip_File_Writer_u();
	~Gzip_File_Writer_u();

	error_t open( const TCHAR*, int compression = -1 );
	error_t write( const void*, long );
	void close();
};

#endif
