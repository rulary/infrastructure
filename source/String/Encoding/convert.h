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
	UnicodeString AnsiToUniocde(const AnsiString&)
	{
		UnicodeString result = {0};

		return result;
	}

	static inline
	AnsiString    UnicodeToAnsi(const UnicodeString&)
	{
		AnsiString result = { 0 };
		return result;
	}
}

#endif	//_ENCODING_CONVERT_H_