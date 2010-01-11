
// Sets up common environment for Shay Green's libraries.
//
// To change configuration options, modify blargg_config.h, not this file.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

// User configuration (allow it to #include "blargg_common.h" normally)
#undef BLARGG_COMMON_H
#include "blargg_config.h"
#define BLARGG_COMMON_H

/* BLARGG_COMPILER_HAS_BOOL: If 0, provides bool support for old compiler. If 1,
   compiler is assumed to support bool. If undefined, availability is determined.
   If errors occur here, add the following line to your config.h file:
	#define BLARGG_COMPILER_HAS_BOOL 1
*/
#ifndef BLARGG_COMPILER_HAS_BOOL
	#if defined (__MWERKS__)
		#if !__option(bool)
			#define BLARGG_COMPILER_HAS_BOOL 0
		#endif
	#elif defined (_MSC_VER)
		#if _MSC_VER < 1100
			#define BLARGG_COMPILER_HAS_BOOL 0
		#endif
	#elif defined (__GNUC__)
		// supports bool
	#elif __cplusplus < 199711
		#define BLARGG_COMPILER_HAS_BOOL 0
	#endif
#endif
#if defined (BLARGG_COMPILER_HAS_BOOL) && !BLARGG_COMPILER_HAS_BOOL
	typedef int bool;
	const bool true  = 1;
	const bool false = 0;
#endif

// BLARGG_NEW is used in place of 'new' to create objects. By default, plain new is used.
// To prevent an exception if out of memory, #define BLARGG_NEW new (std::nothrow)
#ifndef BLARGG_NEW
	#define BLARGG_NEW new
#endif

// Set up boost
#include "boost/config.hpp"
#ifndef BOOST_MINIMAL
	#define BOOST boost
	#ifndef BLARGG_COMPILER_HAS_NAMESPACE
		#define BLARGG_COMPILER_HAS_NAMESPACE 1
	#endif
	#ifndef BLARGG_COMPILER_HAS_BOOL
		#define BLARGG_COMPILER_HAS_BOOL 1
	#endif
#endif

// BOOST::int8_t etc.
#include "boost/cstdint.hpp"

// BOOST_STATIC_ASSERT( expr ): Generates compile error if expr is 0.
#include "boost/static_assert.hpp"

// STATIC_CAST(T,expr): Used in place of static_cast<T> (expr)
#ifndef STATIC_CAST
	#define STATIC_CAST(T,expr) ((T) (expr))
#endif

// blargg_err_t (0 on success, otherwise error string)
#ifndef blargg_err_t
	typedef const char* blargg_err_t;
#endif

#endif

