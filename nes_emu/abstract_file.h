
// Abstract file access interfaces

#ifndef ABSTRACT_FILE_H
#define ABSTRACT_FILE_H

#include <stdio.h>

// Supports reading and finding out how many bytes are remaining
class Data_Reader {
public:
	Data_Reader() { }
	virtual ~Data_Reader() { }
	
	// NULL on success, otherwise error string
	typedef const char* error_t;
	
	// Read at most 'n' bytes. Return number of bytes read, zero or negative
	// if error.
	virtual long read_avail( void*, long n ) = 0;
	
	// Read exactly 'n' bytes (error if fewer are available).
	virtual error_t read( void*, long );
	
	// Number of bytes remaining
	virtual long remain() const = 0;
	
	// Skip forwards by 'n' bytes.
	virtual error_t skip( long n );
	
	// to do: bytes remaining = LONG_MAX when unknown?
	
private:
	// noncopyable
	Data_Reader( const Data_Reader& );
	Data_Reader& operator = ( const Data_Reader& );
};

// Adds seeking operations
class File_Reader : public Data_Reader {
public:
	// Size of file
	virtual long size() const = 0;
	
	// Current position in file
	virtual long tell() const = 0;
	
	// Change position in file
	virtual error_t seek( long ) = 0;
	
	virtual long remain() const;
	
	error_t skip( long n );
};

// Limit access to a subset of data
class Subset_Reader : public Data_Reader {
	Data_Reader* in;
	long remain_;
public:
	Subset_Reader( Data_Reader*, long size );
	long remain() const;
	long read_avail( void*, long );
};

// Treat range of memory as a file
class Mem_File_Reader : public File_Reader {
	const char* const begin;
	long pos;
	const long size_;
public:
	Mem_File_Reader( const void*, long size );
	
	long size() const;
	long read_avail( void*, long );
	
	long tell() const;
	error_t seek( long );
};

// File reader based on C FILE
class Std_File_Reader : public File_Reader {
public:
	Std_File_Reader();
	~Std_File_Reader();
	
	error_t open( const char* );
	
	FILE* file() const { return file_; }
	
	// Forward read requests to file. Caller must close file later.
	//void forward( FILE* );
	
	long size() const;
	long read_avail( void*, long );
	
	long tell() const;
	error_t seek( long );
	
	void close();

protected:
	void reset( FILE* f ) { file_ = f; }
	//FILE* owned_file;
private:
	FILE* file_;
};

// Supports writing
class Data_Writer {
public:
	Data_Writer() { }
	virtual ~Data_Writer() { }
	
	typedef const char* error_t;
	
	// Write 'n' bytes. NULL on success, otherwise error string.
	virtual error_t write( const void*, long n ) = 0;
	
	void satisfy_lame_linker_();
private:
	// noncopyable
	Data_Writer( const Data_Writer& );
	Data_Writer& operator = ( const Data_Writer& );
};

class Std_File_Writer : public Data_Writer {
public:
	Std_File_Writer();
	~Std_File_Writer();
	
	error_t open( const char* );
	
	FILE* file() const { return file_; }
	
	// Forward writes to file. Caller must close file later.
	//void forward( FILE* );
	
	error_t write( const void*, long );
	
	void close();
	
protected:
	void reset( FILE* f ) { file_ = f; }
private:
	FILE* file_;
	error_t open( const char* path, int ignored ) { return open( path ); }
	friend class Auto_File_Writer;
};

// Write data to memory
class Mem_Writer : public Data_Writer {
	char* data_;
	long size_;
	long allocated;
	enum { expanding, fixed, ignore_excess } mode;
public:
	// Keep all written data in expanding block of memory
	Mem_Writer();
	
	// Write to fixed-size block of memory. If ignore_excess is false, returns
	// error if more than 'size' data is written, otherwise ignores any excess.
	Mem_Writer( void*, long size, int ignore_excess = 0 );
	
	error_t write( const void*, long );
	
	// Pointer to beginning of written data
	char* data() { return data_; }
	
	// Number of bytes written
	long size() const { return size_; }
	
	~Mem_Writer();
};

// Written data is ignored
class Null_Writer : public Data_Writer {
public:
	error_t write( const void*, long );
};

// Auto_File to use in place of Data_Reader&/Data_Writer&, allowing a normal
// file path to be used in addition to a Data_Reader/Data_Writer.

class Auto_File_Reader {
public:
	Auto_File_Reader()                      : data(  0 ), path( 0 ) { }
	Auto_File_Reader( Data_Reader& r )      : data( &r ), path( 0 ) { }
#ifndef DISABLE_AUTO_FILE
	Auto_File_Reader( const char* path_ )   : data(  0 ), path( path_ ) { }
#endif
	Auto_File_Reader( Auto_File_Reader const& );
	Auto_File_Reader& operator = ( Auto_File_Reader const& );
	~Auto_File_Reader();
	const char* open();
	
	int operator ! () const { return !data; }
	Data_Reader* operator -> () const { return  data; }
	Data_Reader& operator *  () const { return *data; }
private:
	/* mutable */ Data_Reader* data;
	const char* path;
};

class Auto_File_Writer {
public:
	Auto_File_Writer()                      : data(  0 ), path( 0 ) { }
	Auto_File_Writer( Data_Writer& r )      : data( &r ), path( 0 ) { }
#ifndef DISABLE_AUTO_FILE
	Auto_File_Writer( const char* path_ )   : data(  0 ), path( path_ ) { }
#endif
	Auto_File_Writer( Auto_File_Writer const& );
	Auto_File_Writer& operator = ( Auto_File_Writer const& );
	~Auto_File_Writer();
	const char* open();
	const char* open_comp( int level = -1 ); // compress output if possible
	
	int operator ! () const { return !data; }
	Data_Writer* operator -> () const { return  data; }
	Data_Writer& operator *  () const { return *data; }
private:
	/* mutable */ Data_Writer* data;
	const char* path;
};

inline Auto_File_Reader& Auto_File_Reader::operator = ( Auto_File_Reader const& r )
{
	data = r.data;
	path = r.path;
	((Auto_File_Reader*) &r)->data = 0;
	return *this;
}
inline Auto_File_Reader::Auto_File_Reader( Auto_File_Reader const& r ) { *this = r; }

inline Auto_File_Writer& Auto_File_Writer::operator = ( Auto_File_Writer const& r )
{
	data = r.data;
	path = r.path;
	((Auto_File_Writer*) &r)->data = 0;
	return *this;
}
inline Auto_File_Writer::Auto_File_Writer( Auto_File_Writer const& r ) { *this = r; }

// Gzip_File

// Get size of gzipped file data (or size of file if not gzipped). NULL
// on success, otherwise error string.
const char* get_gzip_eof( const char* path, long* eof_out );

class Gzip_File_Reader : public File_Reader {
	void* file_;
	long size_;
public:
	Gzip_File_Reader();
	~Gzip_File_Reader();
	
	error_t open( const char* );
	
	long size() const;
	long read_avail( void*, long );
	
	long tell() const;
	error_t seek( long );
	
	void close();
};

class Gzip_File_Writer : public Data_Writer {
	void* file_;
public:
	Gzip_File_Writer();
	~Gzip_File_Writer();
	
	error_t open( const char*, int compression = -1 );
	error_t write( const void*, long );
	void close();
};

#endif

