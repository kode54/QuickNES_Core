#ifndef _guid_container_h_
#define _guid_container_h_

#include <windows.h>

class guid_container
{
public:
	~guid_container() {}

	virtual unsigned add( const GUID & ) = 0;

	virtual void remove( const GUID & ) = 0;

	virtual bool get_guid( unsigned, GUID & ) = 0;
};

guid_container * create_guid_container();

#endif