#ifndef _DUMP_BUFFER_H_
#define _DUMP_BUFFER_H_

#include "string/encoding/convert.h"

#include <tchar.h>

class CDumpBuffer
{
public:
	CDumpBuffer();
	~CDumpBuffer();
	void Clear();
	void Printf(BOOL bIsLog, TCHAR* format, ...);

private:
	enum { BUFFER_SIZE = 64 * 1024 };
	char buffer[BUFFER_SIZE];
	char* current;
	CRITICAL_SECTION DumpBufferSync;
	HANDLE hLogFile;
};


#endif // #ifndef _DUMP_BUFFER_H_
