#ifndef _DEFINE_H_
#define _DEFINE_H_

#include "buildconfig.h"

#if defined(_MSC_VER) || defined(__INTER_COMPILER) || defined(__ICC)
#   ifndef _WINDOWS_
#       define _WINDOWS_
#   endif
#endif

#if defined(_WIN64)
#   ifndef X64
#       define    X64
#   endif
#endif


#if defined(_MSC_VER)
#   define OVERRIDE override
#else
#   define OVERRIDE
#endif

#if defined(_DEBUG) || defined(DEBUG)
#   include <assert.h>
#   define  ASSERT( p ) assert((p))
#else
#   define  ASSERT( p )
#endif

#endif
