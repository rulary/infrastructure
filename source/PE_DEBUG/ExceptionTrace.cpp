
#include <windows.h>
#include <tchar.h>
#include "ExceptionTrace.h"

#include "StackWalker.h"

CDumpBuffer DumpBuffer;
HANDLE      g_ProcessHandle;

#ifdef X64
class MyStackWalker : public StackWalker
{
	TCHAR   buff[1024];
public:
	MyStackWalker() : StackWalker(StackWalker::RetrieveLine | StackWalker::RetrieveModuleInfo) {}
	MyStackWalker(DWORD dwProcessId, HANDLE hProcess) : StackWalker(dwProcessId, hProcess) {}
	virtual void OnOutput(LPCSTR szText) 
	{ 
#ifdef UNICODE
		{
			ZeroMemory(buff, sizeof(buff));
			::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szText, strlen(szText), buff, 1024);
			DumpBuffer.Printf(TRUE, _T("%s\r\n"), buff);
		}
#else
		DumpBuffer.Printf(TRUE, _T("%s\r\n"), szText); 
#endif
		StackWalker::OnOutput(szText); 
	}
};

MyStackWalker st;
#endif

void write_module_name(CDumpBuffer& dumpBuffer, HMODULE hInstance, DWORD64 program_counter)
{
	TCHAR szModuleName[MAX_PATH];
	//	DWORD dwTemp;
	if (hInstance && GetModuleFileName(hInstance, szModuleName, sizeof(szModuleName)))
	{
		int i = _tcslen(szModuleName);
		for (; i >= 0; i--)
		{
			if (szModuleName[i] == TCHAR('\\'))
				break;
		}
		dumpBuffer.Printf(TRUE, _T("%s!"), &szModuleName[i + 1]);

	}
	else
	{
		dumpBuffer.Printf(TRUE, _T("Unknown module!"));
	}
}

void write_function_name(CDumpBuffer& dumpBuffer, HANDLE process, DWORD64 program_counter) {

	SYMBOL_INFO_PACKAGE sym = { sizeof(sym) };
	//	::RtlZeroMemory(&sym,sizeof(sym));
	//	sym.si.SizeOfStruct = sizeof(sym.si) ;
	sym.si.MaxNameLen = MAX_SYM_NAME;

	DWORD64 dqTemp = 0;

	if (SymFromAddr(process, program_counter, &dqTemp, &sym.si))
	{
		//		if(sym.si.NameLen)
		dumpBuffer.Printf(TRUE, _T("%s( )"), sym.si.Name);
		//		else
		//			dumpBuffer.Printf(TRUE,"NULL_%s()",sym.si.Name);
	}
	else
	{

		///DWORD error = GetLastError();
		//dumpBuffer.Printf(TRUE, _T("SymFromAddr returned error : %d "), error);
		dumpBuffer.Printf(TRUE, _T("Unknown function"));
	}
}

void write_file_and_line(CDumpBuffer& dumpBuffer, HANDLE process, DWORD64 program_counter) {

	IMAGEHLP_LINE64 ih_line = { sizeof(IMAGEHLP_LINE64) };
	DWORD dummy = 0;
	//	DWORD	dwTemp;

	if (SymGetLineFromAddr64(process, program_counter, &dummy, &ih_line)) {
		dumpBuffer.Printf(TRUE, _T("|%s:%d"), ih_line.FileName, ih_line.LineNumber);
	}

}

void StackDump(CDumpBuffer& DumpBuffer, REGISTER EBP, REGISTER EIP, REGISTER ESP, int dumpLen)
{
	// The structure of the stack frames is the following:
	// EBP -> parent stack frame EBP
	//        return address for this call ( = caller )
	// The chain can be navigated iteratively; the
	// initial value of EBP is given by the parameter.
	// If EIP != 0, then before we dump the call stack, we
	// dump the same information for EIP. Basically, we
	// consider EIP as the first element of the call stack.
	BOOL EBP_OK = TRUE;
	HINSTANCE hLastInstance = (HINSTANCE)-1;
	if (NULL != EBP)
	{
		do
		{
			// Check if EBP is a good address
			// I'm expecting a standard stack frame, so
			// EPB must be aligned on a DWORD address
			// and it should be possible to read 8 bytes
			// starting at it (next EBP, caller).
			if ((DWORD)EBP & 3)
				EBP_OK = FALSE;
			if (EBP_OK && IsBadReadPtr((void*)EBP, 2 * sizeof(REGISTER)))
				EBP_OK = FALSE;
			if (EBP_OK)
			{
				BYTE* caller = EIP ? (BYTE*)EIP : *((BYTE**)EBP + 1);
				EBP = EIP ? EBP : *(REGISTER*)EBP;

				if (EIP)
					EIP = 0; // So it is considered just once
				// Get the instance handle of the module where caller belongs to
				MEMORY_BASIC_INFORMATION mbi;
				VirtualQuery(caller, &mbi, sizeof(mbi));
				// The instance handle is equal to the allocation base in Win32
				HINSTANCE hInstance = (HINSTANCE)mbi.AllocationBase;
				// If EBP is valid, hInstance is not 0
				if (hInstance == 0)
					EBP_OK = FALSE;
				else
					//	PE_debug.DumpDebugInfo( CDumpBuffer, caller, hInstance ) ;
				{
					if (hLastInstance != hInstance)
					{
						DumpBuffer.Printf(TRUE, _T("-----------------------------------------------------------------------------------------\r\n"));
						hLastInstance = hInstance;
					}
					DumpBuffer.Printf(TRUE, _T("%p|"), caller);
					write_module_name(DumpBuffer, hInstance, (DWORD64)caller);
					write_function_name(DumpBuffer, g_ProcessHandle, (DWORD64)caller);
					write_file_and_line(DumpBuffer, g_ProcessHandle, (DWORD64)caller);
					DumpBuffer.Printf(TRUE, _T("\r\n"));
				}
			}
			else
				break; // End of the call chain
		} while (TRUE);
	}
	//Dump All Stack Data
	if (NULL != ESP)
	{
		LPDWORD lpStackPointor = (LPDWORD)ESP;
		int dumped = 0;
		do{
			DumpBuffer.Printf(TRUE, _T("|%08X:"), lpStackPointor);
			if (!IsBadReadPtr(lpStackPointor, sizeof(DWORD)))
			{
				DWORD data = *lpStackPointor;
				DumpBuffer.Printf(TRUE, _T(" %08X|"), data);
				// Get the instance handle of the module where caller belongs to
				MEMORY_BASIC_INFORMATION mbi;
				VirtualQuery((LPCVOID)*lpStackPointor, &mbi, sizeof(mbi));
				// The instance handle is equal to the allocation base in Win32
				HINSTANCE hInstance = (HINSTANCE)mbi.AllocationBase;
				if (hInstance == 0)
					goto skip;
				write_module_name(DumpBuffer, hInstance, (DWORD64)*lpStackPointor);
				write_function_name(DumpBuffer, g_ProcessHandle, (DWORD64)*lpStackPointor);
				write_file_and_line(DumpBuffer, g_ProcessHandle, (DWORD64)*lpStackPointor);

				dumped++;
				if ((DWORD)dumped >= dumpLen)
					break;
			}
			else
				break;
		skip:
			DumpBuffer.Printf(TRUE, _T("\r\n"));
			lpStackPointor++;
		} while (TRUE);

		DumpBuffer.Printf(TRUE, _T("\r\n"));
	}

}

void DumpExceptContex(CDumpBuffer& dumpBuffer, LPEXCEPTION_POINTERS e)
{
	SYSTEMTIME sTime;
	TCHAR szModuleName[MAX_PATH] = {0};

	::GetLocalTime(&sTime);
	::GetModuleFileName(GetModuleHandle(NULL), szModuleName, sizeof(szModuleName));

	dumpBuffer.Printf(TRUE, _T("\r\n\r\n[UnHandled Exception] Dumped For \r\n%s\r\n"), szModuleName);
	dumpBuffer.Printf(TRUE, _T("Dump Date : %02d-%02d-%02d \t %02d:%02d:%02d \r\n"), \
		sTime.wYear, sTime.wDay, sTime.wMonth, \
		sTime.wHour, sTime.wMinute, sTime.wSecond);

#ifdef X64
	dumpBuffer.Printf(TRUE, _T("\r\n Address :0x%016X \t "), e->ExceptionRecord->ExceptionAddress);
#else
	dumpBuffer.Printf(TRUE, _T("\r\n Address :0x%08X \t "), e->ExceptionRecord->ExceptionAddress);
#endif

	DumpExceptionDescr(DumpBuffer, e->ExceptionRecord->ExceptionCode);
	if (EXCEPTION_ACCESS_VIOLATION == e->ExceptionRecord->ExceptionCode)
	{
		dumpBuffer.Printf(TRUE, _T("\r\nAttemp to %s Data At address %p"), e->ExceptionRecord->ExceptionInformation[0] ? _T("write") : _T("read"), e->ExceptionRecord->ExceptionInformation[1]);
	}

#ifdef X64
	dumpBuffer.Printf(TRUE,_T("\r\nRIP=0x%016X \r\nRAX=0x%016X, RCX=0x%016X \r\nRBX=0x%016X, RDX=0x%016X \r\nRSI=0x%016X, RDI=0x%016X \r\nRSP=0x%016X, RBP=0x%016X \r\n"),\
		e->ContextRecord->Rip,e->ContextRecord->Rax,e->ContextRecord->Rcx,e->ContextRecord->Rbx,e->ContextRecord->Rdx,e->ContextRecord->Rsi,\
		e->ContextRecord->Rdi,e->ContextRecord->Rsp,e->ContextRecord->Rbp);
#else
	dumpBuffer.Printf(TRUE, _T("\r\nEIP=0x%08X \r\nEAX=0x%08X, ECX=0x%08X, EBX=0x%08X, EDX=0x%08X, \r\nESI=0x%08X, EDI=0x%08X, ESP=0x%08X, EBP=0x%08X \r\n"), \
		e->ContextRecord->Eip, e->ContextRecord->Eax, e->ContextRecord->Ecx, e->ContextRecord->Ebx, e->ContextRecord->Edx, e->ContextRecord->Esi, \
		e->ContextRecord->Edi, e->ContextRecord->Esp, e->ContextRecord->Ebp);
#endif
}

void DumpExceptCallsStack(CDumpBuffer& dumpBuffer, REGISTER EBP, REGISTER EIP, CONTEXT* ctx)
{
	TCHAR* separator = _T("============================================================================================\r\n");
	dumpBuffer.Printf(TRUE, _T("\r\nCall stack:\r\n"));
	dumpBuffer.Printf(TRUE, separator);
#ifndef X64
	StackDump(dumpBuffer, EBP, EIP);
#else
	st.ShowCallstack(GetCurrentThread(), ctx);
#endif
	dumpBuffer.Printf(TRUE, separator);
}
void DumpExceptStack(CDumpBuffer& dumpBuffer, REGISTER ESP)
{
	TCHAR* separator = _T("============================================================================================\r\n");
	dumpBuffer.Printf(TRUE, _T("\r\nStack Dumped:\r\n"));
	dumpBuffer.Printf(TRUE, separator);
	StackDump(dumpBuffer, NULL, NULL, ESP);
	dumpBuffer.Printf(TRUE, separator);
}

struct	ExceptionCodeDescr
{
	DWORD code;
	TCHAR* descr;
};

#define CODE_DESCR( c ) { c, _T(#c) }

#define MSVCPP_EXCEPTION 0xE06D7363
static ExceptionCodeDescr exceptionTable[] =
{
	CODE_DESCR(EXCEPTION_ACCESS_VIOLATION),
	CODE_DESCR(EXCEPTION_ARRAY_BOUNDS_EXCEEDED),
	CODE_DESCR(EXCEPTION_BREAKPOINT),
	CODE_DESCR(EXCEPTION_DATATYPE_MISALIGNMENT),
	CODE_DESCR(EXCEPTION_FLT_DENORMAL_OPERAND),
	CODE_DESCR(EXCEPTION_FLT_DIVIDE_BY_ZERO),
	CODE_DESCR(EXCEPTION_FLT_INEXACT_RESULT),
	CODE_DESCR(EXCEPTION_FLT_INVALID_OPERATION),
	CODE_DESCR(EXCEPTION_FLT_OVERFLOW),
	CODE_DESCR(EXCEPTION_FLT_STACK_CHECK),
	CODE_DESCR(EXCEPTION_FLT_UNDERFLOW),
	CODE_DESCR(EXCEPTION_GUARD_PAGE),
	CODE_DESCR(EXCEPTION_ILLEGAL_INSTRUCTION),
	CODE_DESCR(EXCEPTION_IN_PAGE_ERROR),
	CODE_DESCR(EXCEPTION_INT_DIVIDE_BY_ZERO),
	CODE_DESCR(EXCEPTION_INT_OVERFLOW),
	CODE_DESCR(EXCEPTION_INVALID_DISPOSITION),
	CODE_DESCR(EXCEPTION_NONCONTINUABLE_EXCEPTION),
	CODE_DESCR(EXCEPTION_PRIV_INSTRUCTION),
	CODE_DESCR(EXCEPTION_SINGLE_STEP),
	CODE_DESCR(EXCEPTION_STACK_OVERFLOW),
	// might be extended with compiler-specific knowledge of
	// "C++ Exception" code[s]
	CODE_DESCR(MSVCPP_EXCEPTION)
};

void DumpExceptionDescr(CDumpBuffer& dumpBuffer, DWORD code)
{
	const TCHAR* descr = NULL;
	int n = sizeof(exceptionTable) / sizeof(ExceptionCodeDescr);
	for (int i = 0; i < n; i++)
	if (exceptionTable[i].code == code)
	{
		descr = exceptionTable[i].descr;
		break;
	}
	if (descr)
		dumpBuffer.Printf(TRUE, _T("(%08X) %s \r\n"), code, descr);
	else
		dumpBuffer.Printf(TRUE, _T("(%08X) Description unavailable\r\n"), code);
}

LPTOP_LEVEL_EXCEPTION_FILTER lpOldFilter = NULL;

LONG __stdcall ExceptionTraceFilter(LPEXCEPTION_POINTERS e)
{
	DumpBuffer.Clear();

	DumpExceptContex(DumpBuffer, e);

	__try{
#ifdef X64
		DumpExceptCallsStack(DumpBuffer, ContextEDI(e->ContextRecord),
			ContextEIP(e->ContextRecord), e->ContextRecord);
#else
		DumpExceptCallsStack(DumpBuffer, ContextEBP(e->ContextRecord),
			ContextEIP(e->ContextRecord));
#endif
		DumpExceptStack(DumpBuffer, ContextESP(e->ContextRecord));
	}
	__except (EXCEPTION_EXECUTE_HANDLER){
	}
#ifdef _POPMSG
	if(IDYES ==::MessageBox(NULL,_T("程序出现未处理的异常，异常信息已记录在目录下的BUGlog文件\n是否要进行调试？(调试要求系统装有调试器)"),_T("Program Falt"),MB_YESNO | MB_ICONINFORMATION))
	{
		if(lpOldFilter)
			return lpOldFilter( e );
		return  EXCEPTION_CONTINUE_SEARCH ;
	}
	else
#endif
#if defined(DEBUG) || defined(_DEBUG)
		return  EXCEPTION_CONTINUE_SEARCH;
#else
		return  EXCEPTION_EXECUTE_HANDLER;
#endif
}

BOOL EnableDebugPrivilege(BOOL bEnable)
{
	//Enabling the debug privilege allows the application to see 
	//information about service application
	BOOL fOK = FALSE;     //Assume function fails
	HANDLE hToken;
	//Try to open this process's acess token
	if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		//Attempt to modify the "Debug" privilege
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;
		::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		fOK = (GetLastError() == ERROR_SUCCESS);
		::CloseHandle(hToken);
	}
	return fOK;
}

void InitExceptionTrace()
{
#ifndef X64
	//EnableDebugPrivilege(TRUE);
	DWORD options = ::SymGetOptions();
	options |= SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS;//SYMOPT_LOAD_LINES |

	::SymSetOptions(options);
	//::MessageBox(NULL,NULL,NULL,MB_OK);
	::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(), ::GetCurrentProcess(), &g_ProcessHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!::SymInitialize(g_ProcessHandle, NULL, TRUE))
	{
		TCHAR buff[128] = { 0 };
		_stprintf_s(buff, 128, _T("SymInitialize Failed : %d"), ::GetLastError());
		::MessageBox(NULL, buff, _T("PE_Debug"), MB_OK | MB_ICONINFORMATION);
	}
#endif
	if (NULL == lpOldFilter)
		lpOldFilter = SetUnhandledExceptionFilter(ExceptionTraceFilter);
	else
		SetUnhandledExceptionFilter(ExceptionTraceFilter);
}