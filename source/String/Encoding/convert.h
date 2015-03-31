#ifndef _ENCODING_CONVERT_H_
#define _ENCODING_CONVERT_H_

#include "datatype.h"

struct UnicodeString
{
	uint16_t	len;
	uint16_t	maxLength;
	wchar_t*	buffer;
};

struct AnsiString
{
	uint16_t	len;
	uint16_t	maxLength;
	char*		buffer;
};

namespace StringCodeConvert
{
	static inline
	void FreeStringBuff(AnsiString& ansi)
	{
		delete[] ansi.buffer;
	}

	static inline
	void FreeStringBuff(UnicodeString& unicode)
	{
		delete[] unicode.buffer;
	}

	static inline
	UnicodeString AnsiToUtf16(const AnsiString& ansi)
	{
		UnicodeString result = {0};

		int len = ::MultiByteToWideChar(CP_ACP, 0, ansi.buffer, ansi.len, NULL, 0);
		if (len == 0)
			return result;

		int capacity = len + 1;
		capacity = capacity + (16 - capacity % 16);

		result.buffer = new wchar_t[capacity];
		result.maxLength = capacity;

		len = ::MultiByteToWideChar(CP_ACP, 0, ansi.buffer, ansi.len, result.buffer, capacity);
		if (len == 0)
		{
			delete[] result.buffer;
			result.buffer = nullptr;
			return result;
		}

		result.len = len;

		return result;
	}

	static inline
	AnsiString    Utf16ToAnsi(const UnicodeString& unicode)
	{
		AnsiString result = { 0 };

		int len = ::WideCharToMultiByte(CP_ACP, 0, unicode.buffer, unicode.len, NULL, 0, NULL, NULL);
		if (len == 0)
			return result;

		int capacity = len + 1;
		capacity = capacity + (16 - capacity % 16);

		result.buffer = new char[capacity];
		result.maxLength = capacity;

		len = ::WideCharToMultiByte(CP_ACP, 0, unicode.buffer, unicode.len, result.buffer, capacity, NULL, NULL);
		if (len == 0)
		{
			delete[] result.buffer;
			result.buffer = nullptr;
			return result;
		}

		result.len = len;

		return result;
	}

	static inline
	AnsiString    Utf16ToUtf8(const UnicodeString& unicode)
	{
		AnsiString result = { 0 };

		int len = ::WideCharToMultiByte(CP_UTF8, 0, unicode.buffer, unicode.len, NULL, 0, NULL, NULL);
		if (len == 0)
			return result;

		int capacity = len + 1;
		capacity = capacity + (16 - capacity % 16);

		result.buffer = new char[capacity];
		result.maxLength = capacity;

		len = ::WideCharToMultiByte(CP_UTF8, 0, unicode.buffer, unicode.len, result.buffer, capacity, NULL, NULL);
		if (len == 0)
		{
			delete[] result.buffer;
			result.buffer = nullptr;
			return result;
		}

		result.len = len;

		return result;
	}

	static inline
	UnicodeString Utf8ToUtf16(const AnsiString& utf8)
	{
		UnicodeString result = { 0 };

		int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.buffer, utf8.len, NULL, 0);
		if (len == 0)
			return result;

		int capacity = len + 1;
		capacity = capacity + (16 - capacity % 16);

		result.buffer = new wchar_t[capacity];
		result.maxLength = capacity;

		len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.buffer, utf8.len, result.buffer, capacity);
		if (len == 0)
		{
			delete[] result.buffer;
			result.buffer = nullptr;
			return result;
		}

		result.len = len;

		return result;
	}

	static inline
	AnsiString    Utf8ToAnsi(const AnsiString& utf8)
	{
		auto utf16 = Utf8ToUtf16(utf8);

		if (utf16.buffer && utf16.len > 0)
		{
			auto ansi = Utf16ToAnsi(utf16);
			FreeStringBuff(utf16);
			return ansi;
		}

		return AnsiString();
	}

	static inline
	AnsiString    AnsiToUtf8(const AnsiString& ansi)
	{
		auto utf16 = AnsiToUtf16(ansi);

		if (utf16.buffer && utf16.len > 0)
		{
			auto ansi = Utf16ToUtf8(utf16);
			FreeStringBuff(utf16);
			return ansi;
		}

		return AnsiString();
	}
}

#endif	//_ENCODING_CONVERT_H_