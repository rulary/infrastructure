// CDumpBuffer.H

// Author: Dr. Carlo Pescio
// Eptacom Consulting
// Via Bernardo Forte 2-3
// 17100 Savona - ITALY
// Fax +39-19-854761
// email pescio@programmers.net


#ifndef DUMP_BUFFER_


#define DUMP_BUFFER_
#include "windows.h"

//#define LOGFILENAME "PE_DEBUG.log"

class CDumpBuffer
  {
  public :
    CDumpBuffer();
	~CDumpBuffer();
    void Clear() ;
	void Printf(BOOL bIsLog, TCHAR* format, ...);
    void SetWindowText( HWND hWnd ) ;
  private :
    enum { BUFFER_SIZE = 32000 } ;
    char buffer[ BUFFER_SIZE ] ;
    char* current ;
	CRITICAL_SECTION DumpBufferSync;
	HANDLE hLogFile;
  } ;


#endif // #ifndef DUMP_BUFFER_
