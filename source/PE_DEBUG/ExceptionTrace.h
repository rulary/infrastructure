
#ifndef EXCEPTTRACE_
#define EXCEPTTRACE_


#include <windows.h>
#include "DumpBuffer.h"
#include "Dbghelp.h"
#pragma comment(lib,"Dbghelp.lib")

#if defined(X64) || defined(_X64_) || defined(AMD64) || defined(_AMD64_)
#undef X64

#define X64

#define  REGISTER	DWORD64

#define  ContextEAX(ctx)  (ctx)->Rax
#define  ContextEBX(ctx)  (ctx)->Rbx
#define  ContextECX(ctx)  (ctx)->Rcx
#define  ContextEDX(ctx)  (ctx)->Rdx

#define  ContextEBP(ctx)  (ctx)->Rbp
#define  ContextESP(ctx)  (ctx)->Rsp
#define  ContextEIP(ctx)  (ctx)->Rip

#define  ContextEDI(ctx)  (ctx)->Rdi

#else
#define  REGISTER	DWORD
#define  ContextEAX(ctx)  (ctx)->Eax
#define  ContextEBX(ctx)  (ctx)->Ebx
#define  ContextECX(ctx)  (ctx)->Ecx
#define  ContextEDX(ctx)  (ctx)->Edx

#define  ContextEBP(ctx)  (ctx)->Ebp
#define  ContextESP(ctx)  (ctx)->Esp
#define  ContextEIP(ctx)  (ctx)->Eip

#define  ContextEDI(ctx)  (ctx)->Edi

#endif

#ifdef EXCEPTION_TRACE
// uses a static object to guarantee that
// InitExceptionTraceLibrary is called at the
// beginning of the program, but after the
// RTL has installed its own filter
void InitExceptionTrace();

static class CExceptionTraceInitializer
{
public :
	CExceptionTraceInitializer()
	{ 
		InitExceptionTrace() ;
	}
} ExceptionTraceInitializer ;

#endif //#ifndef EXCEPTION_TRACE
//#define _POPMSG

/*
extern "C" DLLEXPORT void VerboseAssert( const char* file, LONG line ) ;

#ifdef _DEBUG
  #define VASSERT( c )                     \
    if( !(c) )                             \
      VerboseAssert( __FILE__, __LINE__ )
#else
  #define VASSERT( c ) NULL
#endif //#ifdef _DEBUG 

*/

LONG __stdcall ExceptionTraceFilter( LPEXCEPTION_POINTERS e );
void StackDump(CDumpBuffer& DumpBuffer, REGISTER EBP, REGISTER EIP, REGISTER ESP = NULL, int dumpLen = 128);
void DumpExceptContex (CDumpBuffer& dumpBuffer, LPEXCEPTION_POINTERS e );
void write_module_name(CDumpBuffer& dumpBuffer,HMODULE hInstance, DWORD64 program_counter);
void write_function_name(CDumpBuffer& dumpBuffer,HANDLE process, DWORD64 program_counter);
void write_file_and_line(CDumpBuffer& dumpBuffer, HANDLE process, DWORD64 program_counter);
void DumpExceptCallsStack(CDumpBuffer& dumpBuffer, REGISTER EBP, REGISTER EIP, CONTEXT* ctx = NULL);
void DumpExceptStack(CDumpBuffer& dumpBuffer, REGISTER ESP);
void DumpExceptionDescr( CDumpBuffer& dumpBuffer, DWORD code );


#if defined(UNICODE) || defined(_UNICODE)

#define SYMBOL_INFO_PACKAGE		SYMBOL_INFO_PACKAGEW
#define SymFromAddr				SymFromAddrW
#define IMAGEHLP_LINE64			IMAGEHLP_LINEW64
#define SymGetLineFromAddr64	SymGetLineFromAddrW64

#endif

#endif // #ifndef EXCEPTTRACE_
