
#ifndef _MULTIBYTECHAR_H_
#define _MULTIBYTECHAR_H_

#include <Windows.h>
inline
char* LocalToUtf8(const char* szlpLocalChar)
{
	int len = ::MultiByteToWideChar(CP_ACP, 0, szlpLocalChar, -1, NULL, 0);
	if(len <= 0)
		return nullptr;

	wchar_t* lpWchar = new wchar_t[len];
	::MultiByteToWideChar(CP_ACP, 0, szlpLocalChar, -1, lpWchar, len);

	len = ::WideCharToMultiByte(CP_UTF8,0,lpWchar,-1,NULL,0,NULL,NULL);

	char* lpMtChar = new char[len];
	::WideCharToMultiByte(CP_UTF8,0,lpWchar,-1,lpMtChar,len,NULL,NULL);

	delete[] lpWchar;

	return lpMtChar;
}
inline
char* Utf8ToLocal(const char* szlpUtf8)
{
	int len = ::MultiByteToWideChar(CP_UTF8, 0, szlpUtf8, -1, NULL, 0);
	if(len <= 0)
		return nullptr;

	wchar_t* lpWchar = new wchar_t[len];
	::MultiByteToWideChar(CP_UTF8, 0, szlpUtf8, -1, lpWchar, len);

	len = ::WideCharToMultiByte(CP_ACP,0,lpWchar,-1,NULL,0,NULL,NULL);

	char* lpMtChar = new char[len];
	::WideCharToMultiByte(CP_ACP,0,lpWchar,-1,lpMtChar,len,NULL,NULL);

	delete[] lpWchar;

	return lpMtChar;
}
inline
wchar_t * LocalToWideChar(const char* szlpLocalChar)
{
	int len = ::MultiByteToWideChar(CP_ACP, 0, szlpLocalChar, -1, NULL, 0);
	if(len <= 0)
		return nullptr;

	wchar_t* lpWchar = new wchar_t[len];
	::MultiByteToWideChar(CP_ACP, 0, szlpLocalChar, -1, lpWchar, len);

	return lpWchar;
}
inline
wchar_t * Utf8ToWideChar(const char* szlpUtf8)
{
	int len = ::MultiByteToWideChar(CP_UTF8, 0, szlpUtf8, -1, NULL, 0);
	if(len <= 0)
		return nullptr;

	wchar_t* lpWchar = new wchar_t[len];
	::MultiByteToWideChar(CP_UTF8, 0, szlpUtf8, -1, lpWchar, len);

	return lpWchar;
}
inline
char * WideCharToLocal(const wchar_t* szlpWchar)
{
	int len = ::WideCharToMultiByte(CP_ACP,0,szlpWchar,-1,NULL,0,NULL,NULL);
	if(len <= 0)
		return nullptr;

	char* lpMtChar = new char[len];
	::WideCharToMultiByte(CP_ACP,0,szlpWchar,-1,lpMtChar,len,NULL,NULL);

	return lpMtChar;
}
inline
char * WideCharToUtf8(const wchar_t* szlpWchar)
{
	int len = ::WideCharToMultiByte(CP_UTF8,0,szlpWchar,-1,NULL,0,NULL,NULL);
	if(len <= 0)
		return nullptr;

	char* lpMtChar = new char[len];
	::WideCharToMultiByte(CP_UTF8,0,szlpWchar,-1,lpMtChar,len,NULL,NULL);

	return lpMtChar;
}

#endif 