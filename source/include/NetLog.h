#ifndef _NET_LOG_H_
#define _NET_LOG_H_

#include <stdarg.h>

void printfln(int logLevel, const char* format, ...)
{
    char buff[1024];

    va_list args; va_start(args, format);

    vsprintf_s(buff, 1024, format, args);

    printf("%s \r\n", buff);

    va_end(args);
}

#ifdef  IOCPNETLOG
#   define NLOG_DIRECT(format, ...)	    printfln(100,format,##__VA_ARGS__)
#   define NLOG_VERBOSE(format, ...)	printfln(0,format,##__VA_ARGS__)
#   define NLOG_INFO(format, ...)		printfln(1,format,##__VA_ARGS__)
#   define NLOG_WARNING(format, ...)	printfln(2,format,##__VA_ARGS__)
#   define NLOG_ERROR(format, ...)		printfln(3,format,##__VA_ARGS__)
#else
#   define NLOG_DIRECT(format, ...)
#   define NLOG_VERBOSE(format, ...)
#   define NLOG_INFO(format, ...)
#   define NLOG_WARNING(format, ...)
#   define NLOG_ERROR(format, ...)
#endif

#endif  _NET_LOG_H_