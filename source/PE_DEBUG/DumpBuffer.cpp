// CDumpBuffer.CPP

// Author: Dr. Carlo Pescio
// Eptacom Consulting
// Via Bernardo Forte 2-3
// 17100 Savona - ITALY
// Fax +39-19-854761
// email pescio@programmers.net


#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <tchar.h>
#include "DumpBuffer.h"


CDumpBuffer::CDumpBuffer()
:hLogFile(INVALID_HANDLE_VALUE)
{
	TCHAR szBuff[MAX_PATH], szTemp[32];
	SYSTEMTIME Time;
	::GetLocalTime(&Time);

	::wsprintf(szTemp, _T("PE_DEBUG_%02d_%02d.%02d_%02d_%02d.log"), \
		Time.wMonth, Time.wDay, Time.wHour, Time.wMinute, Time.wSecond);
	::GetFullPathName(szTemp, sizeof(szBuff) / sizeof(szBuff[0]), szBuff, NULL);
	hLogFile = ::CreateFile(szBuff, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, \
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	InitializeCriticalSection(&DumpBufferSync);

#if defined(UNICODE) || defined(_UNICODE)
	// write utf-16 byte order mark in le
	DWORD utf16le = 0xfeff;
	DWORD dwTemp = 0;
	::WriteFile(hLogFile, &utf16le, 2, &dwTemp, NULL);
#endif

	Clear();
}
CDumpBuffer ::~CDumpBuffer()
{
	DeleteCriticalSection(&DumpBufferSync);
	CloseHandle(hLogFile);
}

void CDumpBuffer::Clear()
{
	current = buffer;
}


void CDumpBuffer::Printf(BOOL bIsLog, TCHAR* format, ...)
{
	TCHAR szBuff[1024];
	if (bIsLog && hLogFile != INVALID_HANDLE_VALUE)
	{
		__try{
			va_list argPtr;
			va_start(argPtr, format);
			int count = _vstprintf_s(szBuff, 1024, format, argPtr);
			va_end(argPtr);
			DWORD dwTemp;
			::EnterCriticalSection(&DumpBufferSync);
			::SetFilePointer(hLogFile, 0, 0, FILE_END);
			::WriteFile(hLogFile, szBuff, count * sizeof(TCHAR), &dwTemp, NULL);
		}
		__except (EXCEPTION_EXECUTE_HANDLER){
		}
		::LeaveCriticalSection(&DumpBufferSync);
	}

	/*
	// protect against obvious buffer overflow
	::EnterCriticalSection(&DumpBufferSync);

	auto c = current - buffer;
	if (c < BUFFER_SIZE)
	{
		__try{
			va_list argPtr;
			va_start(argPtr, format);
			int count = _vstprintf_s((TCHAR*)current, (BUFFER_SIZE - c) / sizeof(TCHAR), format, argPtr);
			va_end(argPtr);
			current += count * sizeof(TCHAR);
		}
		__except (EXCEPTION_EXECUTE_HANDLER){
		}
	}
	::LeaveCriticalSection(&DumpBufferSync);
	*/
}


void CDumpBuffer::SetWindowText(HWND hWnd)
{
	::EnterCriticalSection(&DumpBufferSync);
	__try{
		SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)buffer);
	}
	__except (EXCEPTION_EXECUTE_HANDLER){
	}
	::LeaveCriticalSection(&DumpBufferSync);
}

