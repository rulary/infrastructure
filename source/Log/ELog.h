#ifndef _LOG_ELOG_H_
#define _LOG_ELOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <memory>
#include <vector>
#include <assert.h>

namespace std
{
    class exception;
}

namespace infrastructure{
    class std::exception;
    using namespace std;

    enum LogLevel
    {
        ELOG_VERBOSE = -1,
        ELOG_INFO = 0,
        ELOG_WARNING,
        ELOG_ERROR,

        ELOG_DIRECT
    };
    struct ILog
    {
        virtual void print(LogLevel lvl, char* szFormat, ...) = 0;
        virtual void println(LogLevel lvl, char* szFormat, ...) = 0;

        virtual void printException(std::exception*) = 0;

        virtual ~ILog() = 0 {};

    public:
        virtual void _write(LogLevel lvl, char* szString, bool isLineOK = true) = 0;
    };

    struct ILogWriter
    {
        virtual void write(LogLevel lvl, char*) = 0;
        virtual ~ILogWriter() = 0 {};
    };

    struct ILogPrefix
    {
        virtual void writePrefix(LogLevel lvl, char* buff, int guffLen) = 0;
        virtual ~ILogPrefix() = 0 {}
    };

    template<typename S>	//W: write stream
    class StreamWriter : public ILogWriter
    {
    public:
        StreamWriter(S wstream) :stream(wstream) {}
        virtual ~StreamWriter(){}
    public:
        void write(LogLevel lvl, char* szMsg) override
        {
            stream << szMsg;
        }
    private:
        S stream;
    };

    class WriterStreamNop
    {
    public:
        WriterStreamNop & operator<< (char*){}
    };

    //基本日志对象
    class Log : public ILog
    {
        shared_ptr<ILogWriter> _writer;
    protected:
        bool _isNewLine;
    protected:
        Log() {}
    public:
        Log(shared_ptr<ILogWriter> writer)
            :_writer(writer)
            , _isNewLine(true){}
    public:
        void print(LogLevel lvl, char* szFormat, ...) override
        {
            va_list argptr;
            va_start(argptr, szFormat);

            char buff[4096];
            //memset(buff,0,4096);

            vsprintf_s(buff, 4094, szFormat, argptr);
            va_end(argptr);
            _write(lvl, buff, false);
        }

        void println(LogLevel lvl, char* szFormat, ...) override
        {
            va_list argptr;
            va_start(argptr, szFormat);
            char buff[4096];
            //memset(buff,0,4096);

            vsprintf_s(buff, 4094, szFormat, argptr);
            va_end(argptr);

            buff[4094] = 0;
            int len = (int)strlen(buff);
            if (len <= 0)
                return;
#ifndef _LINUX_
            buff[len] = '\r';
            buff[len + 1] = '\n';
            buff[len + 2] = 0;
#else
            buff[len ] = '\n';
            buff[len + 1] = 0;
#endif	
            _write(lvl, buff, true);
        }

        virtual void printException(std::exception*){ assert(false); }

    public:

        void _write(LogLevel lvl, char* szString, bool isLineOK /*= true*/) override
        {
            if (_isNewLine)
                _isNewLine = false;
            _writer->write(lvl, szString);

            _isNewLine = isLineOK;
        }
    };

    //为日志添加前缀
    template<typename PrefixFactory>
    class PrefixedLog :public Log
    {
    private:
        shared_ptr<ILog> _logger;
        ILogPrefix* _prefix;
    private:
        PrefixedLog();
    public:
        PrefixedLog(shared_ptr<ILog> logger)
            :_logger(logger)
        {
            assert(logger != nullptr);
            _prefix = PrefixFactory::CreateLogPrefix();
            assert(_prefix != nullptr);
        }

        ~PrefixedLog()
        {
            if (_prefix)
            {
                PrefixFactory::ReleaseLogPrefix(_prefix);
            }
        }
    public:
        void _write(LogLevel lvl, char *szString, bool isLineOK /*= true*/) override
        {
            if (_isNewLine)
            {
                _isNewLine = false;
                if (_prefix && lvl < ELOG_DIRECT)
                {
                    char buff[1024];
                    memset(buff, 0, 1024);
                    _prefix->writePrefix(lvl, buff, 1024);
                    buff[1023] = 0;
                    _logger->_write(lvl, buff, false);
                }
            }

            _logger->_write(lvl, szString, isLineOK);

            _isNewLine = isLineOK;
        }
    };

    //对组合的所有的日志对象都进行日志操作
    class ConcurrentLogs : public Log
    {
    private:
        vector<shared_ptr<ILog>> _loggers;

    private:
        ConcurrentLogs();

    public:
        ConcurrentLogs(vector<shared_ptr<ILog>> loggers)
            :_loggers(loggers)
        {}

    public:
        void _write(LogLevel lvl, char* szString, bool isLineOK /*= true*/) override
        {
            if (_isNewLine)
                _isNewLine = false;

            vector<shared_ptr<ILog>>::iterator it;
            for (it = _loggers.begin(); it != _loggers.end(); it++)
            {
                shared_ptr<ILog> logger = *it;
                logger->_write(lvl, szString, isLineOK);
            }

            _isNewLine = isLineOK;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    //等级大于等于时 才进行日志操作
    class LogLevelGEQ : public Log
    {
    private:
        shared_ptr<ILog> _logIfTrue;
        LogLevel         _lvl;
    public:
        LogLevelGEQ();
    public:
        LogLevelGEQ(LogLevel lvl, shared_ptr<ILog> logger)
            :_logIfTrue(logger), _lvl(lvl)
        {}

    public:
        void _write(LogLevel lvl, char* szString, bool isLineOK /*= true*/) override
        {
            if (lvl >= _lvl)
                _logIfTrue->_write(lvl, szString, isLineOK);
        }
    };

    //////////////////////////////////////////////////////////////////////////
    //只志记指定等级的日志
    class LogLevelOnly : public Log
    {
    private:
        shared_ptr<ILog> _logIfTrue;
        LogLevel         _lvl;
    public:
        LogLevelOnly();
    public:
        LogLevelOnly(LogLevel lvl, shared_ptr<ILog> logger)
            :_logIfTrue(logger), _lvl(lvl)
        {}

    public:
        void _write(LogLevel lvl, char* szString, bool isLineOK /*= true*/) override
        {
            if (lvl == _lvl || lvl == ELOG_DIRECT)
                _logIfTrue->_write(lvl, szString, isLineOK);
        }
    };
}
#endif