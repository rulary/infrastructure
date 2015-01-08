#ifndef _INFRASTRUCT_DATATYPE_H_
#define _INFRASTRUCT_DATATYPE_H_

#include "defines.h"

#if defined(_WINDOWS_)

#include <windows.h>

//typedef ULONG_PTR   ulong_ptr;
//typedef LONG_PTR    long_ptr;

typedef __int64             int64_t;
typedef unsigned __int64    uint64_t;

typedef __int32             int32_t;
typedef unsigned __int32    uint32_t;


#endif  /* _WINDOWS_ */


#ifdef X64

typedef int64_t     int_t;
typedef uint64_t    uint_t;
typedef int64_t     long_t;
typedef uint64_t    ulong_t;

#else

typedef int32_t     int_t;
typedef uint32_t    uint_t;
typedef int32_t     long_t;
typedef uint32_t    ulong_t;

#endif  /* X64 */

#endif  /* _INFRASTRUCT_DATATYPE_H_ */