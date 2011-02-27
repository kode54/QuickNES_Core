#include "stdafx.h"

#include "sha2.h"

#include <set>

static bool is_greater( FILETIME const& a, FILETIME const& b )
{
	if ( a.dwHighDateTime > b.dwHighDateTime ) return true;
	if ( a.dwHighDateTime == a.dwHighDateTime && a.dwLowDateTime > b.dwLowDateTime ) return true;
	return false;
}

static bool find_database( WTL::CString & p_path, WTL::CString save_path )
{
	FILETIME modification_time = {0,0};
	FILETIME comparison_time;
	WTL::CString temp;
	WTL::CFindFile find;
	temp = save_path + _T( "ZapFC*.fbsv*" );
	if ( find.FindFile( temp ) )
	{
		do
		{
			if ( !find.IsDirectory() && find.GetLastWriteTime( &comparison_time ) && is_greater( comparison_time, modification_time ) )
			{
				p_path = find.GetFilePath();
				modification_time = comparison_time;
			}
		}
		while ( find.FindNextFile() );

		return true;
	}

	TCHAR module_path[ MAX_PATH + 1 ];
	module_path[ MAX_PATH ] = 0;
	if ( GetModuleFileName( NULL, module_path, MAX_PATH ) )
	{
		TCHAR * slash = _tcsrchr( module_path, _T( '\\' ) );
		if ( slash ) slash[ 1 ] = 0;
		temp = WTL::CString( module_path ) + _T( "ZapFC*.fbsv*" );
		if ( find.FindFile( temp ) )
		{
			do
			{
				if ( !find.IsDirectory() && find.GetLastWriteTime( &comparison_time ) && is_greater( comparison_time, modification_time ) )
				{
					p_path = find.GetFilePath();
					modification_time = comparison_time;
				}
			}
			while ( find.FindNextFile() );

			return true;
		}
	}

	return false;
}

wchar_t * database_lookup( void * prg, unsigned prg_size, void * chr, unsigned chr_size, WTL::CString save_path )
{
	WTL::CString database_path;

	if ( !find_database( database_path, save_path ) ) return NULL;

	fex_t * db = NULL;

#ifdef _UNICODE
	char * path8 = fex_wide_to_path( database_path );
	const char * err = fex_open( &db, path8 );
	free( path8 );
#else
	const char * err = fex_open( &db, database_path );
#endif

	if ( err ) return NULL;

	fex_pos_t db_pos = 0;

	while ( !fex_done( db ) )
	{
		const char * name = fex_name( db );
		size_t length = strlen( name );
		if ( length >= 10 && !_strnicmp( name, "ZapFC", 5 ) && !_stricmp( name + length - 5, ".fbsv" ) ) db_pos = fex_tell_arc( db );
		else
		{
			fex_close( db );
			return NULL;
		}
		err = fex_next( db );
		if ( err )
		{
			fex_close( db );
			return NULL;
		}
	}

	if ( !db_pos )
	{
		fex_close( db );
		return NULL;
	}

	err = fex_seek_arc( db, db_pos );
	if ( err )
	{
		fex_close( db );
		return NULL;
	}
	err = fex_stat( db );
	if ( err )
	{
		fex_close( db );
		return NULL;
	}

	std::set<std::string> hash_list;

	char hash_buffer[SHA256_DIGEST_STRING_LENGTH];
	SHA256_CTX ctx;

	SHA256_Init( &ctx );
	SHA256_Update( &ctx, (const uint8_t *)prg, prg_size );
	SHA256_End( &ctx, hash_buffer );
	hash_list.insert( hash_buffer );

	if ( chr_size )
	{
		SHA256_Init( &ctx );
		SHA256_Update( &ctx, (const uint8_t *)chr, chr_size );
		SHA256_End( &ctx, hash_buffer );
		if ( !hash_list.insert( hash_buffer ).second )
		{
			fex_close( db );
			return NULL;
		}
	}

	std::string hash;
	std::set<std::string>::iterator it;
	for ( it = hash_list.begin(); it != hash_list.end(); ++it )
	{
		hash += *it;
	}

	SHA256_Init( &ctx );
	SHA256_Update( &ctx, (const uint8_t *)(hash.c_str()), hash.length() );
	SHA256_End( &ctx, hash_buffer );

	wchar_t * hash16 = blargg_to_wide( hash_buffer );

	char db_buffer[ 1025 ];

	__int64 db_remain = fex_size( db );

	unsigned in_buffer = 0;

	wchar_t * db_entry = NULL;

	std::string db_line;

	while ( db_remain || in_buffer )
	{
		unsigned to_read = 1024 - in_buffer;
		if ( to_read > db_remain ) to_read = (unsigned)db_remain;
		if ( to_read )
		{
			err = fex_read( db, db_buffer + in_buffer, to_read );
			if ( err )
			{
				free( hash16 );
				fex_close( db );
				return NULL;
			}
			db_remain -= to_read;
			in_buffer += to_read;
			db_buffer[ in_buffer ] = 0;
		}
		char * eol = db_buffer;
		while ( unsigned(*eol) >= ' ' ) ++eol;
		*eol = 0;

		db_line += db_buffer;

		if ( db_buffer + 1024 == eol && db_remain )
		{
			in_buffer = 0;
			continue;
		}

		++eol;

		while ( *eol && *eol < ' ' ) ++eol;
		in_buffer = 1024 - ( eol - db_buffer );
		memmove( db_buffer, eol, in_buffer + 1 );

		db_entry = blargg_to_wide( db_line.c_str() );
		if ( !db_entry )
		{
			free( hash16 );
			fex_close( db );
			return NULL;
		}
		db_line.clear();

		const wchar_t * file_size = wcschr( db_entry, 0x2588 ) + 1;
		if ( file_size == (void*)1 )
		{
			free( db_entry );
			free( hash16 );
			fex_close( db );
			return NULL;
		}
		const wchar_t * hash_check = wcschr( file_size, 0x2588 ) + 1;
		if ( hash_check == (void*)1 )
		{
			free( db_entry );
			free( hash16 );
			fex_close( db );
			return NULL;
		}
		const wchar_t * hash_end = wcschr( hash_check, 0x2588 );
		if ( !hash_end )
		{
			free( db_entry );
			free( hash16 );
			fex_close( db );
			return NULL;
		}

		if ( !_wcsnicmp( hash16, hash_check, hash_end - hash_check ) )
		{
			free( hash16 );
			fex_close( db );
			return db_entry;
		}

		free( db_entry );
	}

	free( hash16 );
	fex_close( db );
	return NULL;
}
