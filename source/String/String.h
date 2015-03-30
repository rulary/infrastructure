#ifndef _STRING_IMPLEMENT_H_
#define _STRING_IMPLEMENT_H_
#include "datatype.h"
#include "./Encoding/convert.h"
#include <vector>
#include <functional>

#include <iostream>

#if defined(UNICODE) || defined(_UNICODE) || defined(_UNICODE_)
#define USE_UNICODE		true
#else 
#define USE_UNICODE     false
#endif

#define RoundSize(s, r)  s += r - s % r

// Careful in use : StringEx::string is NOT thread-safe and NOT watching memory insufficient

namespace StringEx{

	struct UnderGround
	{
		bool isUnicode;
		long_t	refCount;
		union
		{
			UnicodeString	unicode;
			AnsiString		ansi;
		};

#if defined(_DEBUG_) || defined(DEBUG) || defined(_DEBUG)
		// for debuging
		UnderGround* next;
		UnderGround* pre;
		static UnderGround* head;

		UnderGround()
		{
			next = pre = nullptr;
			if (head)
			{
				head->pre = this;
				this->next = head;
			}

			head = this;
		}

		~UnderGround()
		{
			if (this->next)
			{
				this->next->pre = this->pre;
			}

			if (this->pre)
			{
				this->pre->next = this->next;
			}
		}
#endif
	};

	class string
	{
		UnderGround*	internalImpl;
	public:
		string();
		string(const char* sz);
		string(const wchar_t* sz);

		string(const string&);
		string(string &&);

		~string();

		string& operator=(const string&);
		string& operator=(string&&);

		bool ToAnsi(char* buff, int& len);
		bool ToUnicode(wchar_t* buff, int& len);

		void clear();

		size_t length();

		string subString(const char* sz);
		string subString(const wchar_t* sz);
		string subString(const string);

		string replace(const char* patten, const char* replacement);
		string replace(const wchar_t* patten, const wchar_t* replacement);
		string replace(const string patten, const char* replacement);
		string replace(const string patten, const wchar_t* replacement);
		string replace(const string patten, const string replacement);
		string replace(const char* patten, const string replacement);
		string replace(const wchar_t* patten, const string replacement);

		std::vector<string> split(const char* patten = " ");
		std::vector<string> split(const char patten = ' ');
		std::vector<string> split(const wchar_t* patten = L" ");
		std::vector<string> split(const wchar_t patten = L' ');
		std::vector<string> splitEx(std::function<bool(char)> f);
		std::vector<string> splitEx(std::function<bool(wchar_t)> f);
		std::vector<string> splitEx(std::function<bool(char, bool&)> f);
		std::vector<string> splitEx(std::function<bool(wchar_t, bool&)> f);

		string operator+(const char* sz);
		string operator+(const wchar_t* sz);
		string operator+(const string str);

		void operator+=(const char* sz);
		void operator+=(const wchar_t* sz);
		void operator+=(const string str);

		// logic operator
		bool operator==(const char* sz);
		bool operator==(const wchar_t* sz);
		bool operator==(const string str);

		bool operator!=(const char* sz);
		bool operator!=(const wchar_t* sz);
		bool operator!=(const string str);

		// helpers
		void format(const char* szFormat, ...);
		void format(const wchar_t* szFormat, ...);

		bool isEmptyOrWhiteSpace();
		// 
		void print(std::ostream& out) const;

		//TODO: Add regular expression patten searching

		// sugur
		string& operator << (const char* sz);
		string& operator << (const wchar_t* sz);
		string& operator << (const string str);

	private:
		void init(bool useUnicode = true, int initBuffLen = 0);
		int getUndergroundCapacity();
		void leaveUnderground();
		void copyUnderBuff(UnderGround* dest, UnderGround* source);

		void writeCopy(int miniCapacity = 0);
		void writeInit();

		void reallocUnderground(int capacity, bool useCopy = true);

		long_t IncUndergoundRef(UnderGround*);
		long_t DecUndergroundRef(UnderGround*);
	};
}

#endif	//_STRING_IMPLEMENT_H_