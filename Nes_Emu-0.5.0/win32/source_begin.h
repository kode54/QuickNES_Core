
// should only be included once at the beginning of a source file
#ifdef SOURCE_BEGIN_H
	#error
#endif
#define SOURCE_BEGIN_H

#include "blargg_source.h"

#ifndef NDEBUG
	#include <stdio.h>
	
	// blargg_source defines these to do nothing by default
	
	#undef dprintf
	#define dprintf printf
	
	#undef check
	#define check( expr ) assert( expr )
#endif

