#ifndef _STD_FILE_H_
#define _STD_FILE_H_

#include <nes_emu/abstract_file.h>
#include <tchar.h>

class Std_File_Reader_u : public Std_File_Reader
{
public:
	error_t open( const TCHAR* path );
};

class Std_File_Writer_u : public Std_File_Writer
{
public:
	error_t open( const TCHAR* path );
};

#endif
