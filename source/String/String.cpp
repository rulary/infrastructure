
#include "String.h"
#include "interlocked.h"

using namespace StringEx;
using namespace StringCodeConvert;

inline
long_t string::IncUndergoundRef(UnderGround* strUnder)
{
	return interlockedInc(&strUnder->refCount);
}

inline
long_t string::DecUndergroundRef(UnderGround* strUnder)
{
	return interlockedDec(&strUnder->refCount);
}

string::string()
: internalImpl(nullptr)
{
	init(USE_UNICODE);
}

string::string(const char* sz)
: internalImpl(nullptr)
{
	init(false);

	auto len = strlen(sz);
	if (len == 0)
		return;

	auto buffSize = len + 1;
	RoundSize(buffSize, 16);
	internalImpl->ansi.buffer = new char[buffSize];
	internalImpl->ansi.maxLength = (uint16_t)buffSize;
	internalImpl->ansi.len = (uint16_t)len;
	strcpy_s(internalImpl->ansi.buffer, buffSize, sz);
}

string::string(const wchar_t* sz)
: internalImpl(nullptr)
{
	init(true);

	auto len = wcslen(sz);
	if (len == 0)
		return;

	auto buffSize = len  + 1;
	RoundSize(buffSize, 16);
	internalImpl->unicode.buffer = new wchar_t[buffSize];
	internalImpl->unicode.maxLength = (uint16_t)buffSize;
	internalImpl->unicode.len = (uint16_t)len;
	wcscpy_s(internalImpl->unicode.buffer, buffSize, sz);
}

string::string(const string& proto)
: internalImpl(proto.internalImpl)
{
	if (internalImpl)
		IncUndergoundRef(internalImpl);
}

string::string(string && temp)
: internalImpl(temp.internalImpl)
{
	temp.internalImpl = nullptr;
}

string::~string()
{
	if (internalImpl)
	{
		leaveUnderground();
		internalImpl = nullptr;
	}
}

string& string::operator=(const string& other)
{
	if (this == &other)
		return *this;

	if (internalImpl)
		leaveUnderground();

	internalImpl = other.internalImpl;
	if (internalImpl)
		IncUndergoundRef(internalImpl);

	return *this;
}

string& string::operator=(string&& temp)
{
	if (internalImpl)
		leaveUnderground();

	internalImpl = temp.internalImpl;
	temp.internalImpl = nullptr;

	return *this;
}

// string 

void string::clear()
{
	writeInit();

	if (internalImpl->isUnicode)
	{
		internalImpl->unicode.len = 0;
		internalImpl->unicode.buffer[0] = 0;
	}
	else
	{
		internalImpl->ansi.len = 0;
		internalImpl->ansi.buffer[0] = 0;
	}
}

size_t string::length() const
{
	if (internalImpl)
		return internalImpl->isUnicode ? internalImpl ->unicode.len : internalImpl->ansi.len;

	return 0;
}

bool string::ToAnsi(char* buff, int& sizeBytes) const
{
	if (!internalImpl)
	{
		sizeBytes = -1;
		return false;
	}

	int strlen = (int)length();
	if (strlen <= 0)
	{
		sizeBytes = -1;
		return false;
	}

	if (internalImpl->isUnicode && strlen * sizeof(wchar_t) > sizeBytes)
	{
		sizeBytes = strlen * sizeof(wchar_t);
		return false;
	}

	if (!internalImpl->isUnicode && strlen > sizeBytes)
	{
		sizeBytes = strlen;
		return false;
	}

	if (!internalImpl->isUnicode)
	{
		bool isConverted = false;
#if USE_UTF8
		auto& ansi = Utf8ToAnsi(internalImpl->utf8);
		isConverted = true;
#else
		auto& ansi = internalImpl->ansi;
#endif
		if (!ansi.buffer)
		{
			sizeBytes = -1;
			return false;
		}

		if (ansi.len > sizeBytes)
		{
			sizeBytes = ansi.len;
			return false;
		}

		memcpy_s(buff, sizeBytes, ansi.buffer, ansi.len);

		if (ansi.len < sizeBytes)
			buff[ansi.len] = 0;

		sizeBytes = ansi.len;

		if (isConverted)
			FreeStringBuff(ansi);

		return true;
	}

	auto ansi = Utf16ToAnsi(internalImpl->unicode);
	if (!ansi.buffer)
	{
		sizeBytes = -1;
		return false;
	}

	if (ansi.len > sizeBytes)
	{
		sizeBytes = ansi.len;
		return false;
	}

	memcpy_s(buff, sizeBytes, ansi.buffer, ansi.len);

	if (ansi.len < sizeBytes)
		buff[ansi.len] = 0;

	sizeBytes = ansi.len;

	FreeStringBuff(ansi);
	return true;
	
}

bool string::ToUtf16(wchar_t* buff, int& sizeBytes) const
{
	if (!internalImpl)
	{
		sizeBytes = -1;
		return false;
	}

	int strlen = (int)length();
	if (strlen <= 0)
	{
		sizeBytes = -1;
		return false;
	}

	if (internalImpl->isUnicode && strlen * sizeof(wchar_t) > sizeBytes)
	{
		sizeBytes = strlen * sizeof(wchar_t);
		return false;
	}

	if (!internalImpl->isUnicode && strlen > sizeBytes)
	{
		sizeBytes = strlen;
		return false;
	}

	if (internalImpl->isUnicode)
	{
		memcpy_s(buff, sizeBytes, internalImpl->unicode.buffer, strlen * sizeof(wchar_t));
		if (strlen * sizeof(wchar_t) < sizeBytes)
			buff[strlen] = 0;
		sizeBytes = strlen * sizeof(wchar_t);

		return true;
	}

#if USE_UTF8
	auto utf16 = Utf8ToUtf16(internalImpl->utf8);
#else
	auto utf16 = AnsiToUtf16(internalImpl->ansi);
#endif

	if (!utf16.buffer)
	{
		sizeBytes = -1;
		return false;
	}

	if (utf16.len * sizeof(wchar_t) > sizeBytes)
	{
		sizeBytes = utf16.len * sizeof(wchar_t);
		FreeStringBuff(utf16);
		return false;
	}

	memcpy_s(buff, sizeBytes, utf16.buffer, utf16.len * sizeof(wchar_t));

	if (utf16.len * sizeof(wchar_t) < sizeBytes)
		buff[utf16.len] = 0;

	sizeBytes = utf16.len * sizeof(wchar_t);

	FreeStringBuff(utf16);

	return true;
}

bool string::ToUtf8(char* buff, int& sizeBytes) const
{
	if (!internalImpl)
	{
		sizeBytes = -1;
		return false;
	}

	int strlen = (int)length();
	if (strlen <= 0)
	{
		sizeBytes = -1;
		return false;
	}

	if (internalImpl->isUnicode && strlen * sizeof(wchar_t) > sizeBytes)
	{
		sizeBytes = strlen * sizeof(wchar_t);
		return false;
	}

	if (!internalImpl->isUnicode && strlen > sizeBytes)
	{
		sizeBytes = strlen;
		return false;
	}

	if (!internalImpl->isUnicode)
	{
		bool isConverted = false;
#if USE_UTF8
		auto& utf8 = internalImpl->utf8;
		
#else
		auto& utf8 = AnsiToUtf8(internalImpl->ansi);
		isConverted = true;
#endif
		if (!utf8.buffer)
		{
			sizeBytes = -1;
			return false;
		}

		if (utf8.len > sizeBytes)
		{
			sizeBytes = utf8.len;
			if (isConverted)
				FreeStringBuff(utf8);
			return false;
		}

		memcpy_s(buff, sizeBytes, utf8.buffer, utf8.len);

		if (utf8.len < sizeBytes)
			buff[utf8.len] = 0;

		sizeBytes = utf8.len;

		if (isConverted)
			FreeStringBuff(utf8);

		return true;
	}

	auto utf8 = Utf16ToUtf8(internalImpl->unicode);
	if (!utf8.buffer)
	{
		sizeBytes = -1;
		return false;
	}

	if (utf8.len > sizeBytes)
	{
		sizeBytes = utf8.len;
		FreeStringBuff(utf8);
		return false;
	}

	memcpy_s(buff, sizeBytes, utf8.buffer, utf8.len);

	if (utf8.len < sizeBytes)
		buff[utf8.len] = 0;

	sizeBytes = utf8.len;

	FreeStringBuff(utf8);
	return true;
}

void string::format(const char* szFormat, ...)
{}

void string::format(const wchar_t* szFormat, ...)
{}


string string::subString(const char* sz) const
{
	return *this;
}

string string::subString(const wchar_t* sz) const
{
	return *this;
}

string string::subString(const string) const
{
	return *this;
}


string string::replace(const char* patten, const char* replacement) const
{
	return *this;
}

string string::replace(const wchar_t* patten, const wchar_t* replacement) const
{
	return *this;
}

string string::replace(const string patten, const char* replacement) const
{
	return *this;
}

string string::replace(const string patten, const wchar_t* replacement) const
{
	return *this;
}

string string::replace(const string patten, const string replacement) const
{
	return *this;
}

string string::replace(const char* patten, const string replacement) const
{
	return *this;
}

string string::replace(const wchar_t* patten, const string replacement) const
{
	return *this;
}


/*
std::vector<string> string::split(const char* patten = " ")
{}

std::vector<string> string::split(const char patten = ' ')
{}

std::vector<string> string::split(const wchar_t* patten = L" ")
{}

std::vector<string> string::split(const wchar_t patten = L' ')
{}

std::vector<string> string::splitEx(std::function<bool(char)> f)
{}
std::vector<string> string::splitEx(std::function<bool(wchar_t)> f)
{}

std::vector<string> string::splitEx(std::function<bool(char, bool&)> f)
{}

std::vector<string> string::splitEx(std::function<bool(wchar_t, bool&)> f)
{}
*/

string string::operator+(const char* sz) const
{
	if(internalImpl->isUnicode)
		return *this;

	string result(*this);

	result += sz;

	return result;
}

string string::operator+(const wchar_t* sz) const
{
	return *this;
}

string string::operator+(const string str) const
{
	return *this;
}

void string::operator+=(const char* sz)
{
	if (internalImpl->isUnicode)
		return;

	writeCopy();

	uint16_t strLen = (uint16_t)strlen(sz);
	auto newsize = strLen + internalImpl->ansi.len + 1;

	RoundSize(newsize, 16);

	writeCopy((int)newsize);

	auto start = &internalImpl->ansi.buffer[internalImpl->ansi.len];
	auto buffSize = internalImpl->ansi.maxLength - strLen;
	memcpy_s(start, newsize, sz, strLen);

	internalImpl->ansi.len += strLen;
	internalImpl->ansi.buffer[internalImpl->ansi.len] = 0;
}

void string::operator+=(const wchar_t* sz)
{}

void string::operator+=(const string str)
{
}

bool string::operator==(const char* sz) const
{
	if (internalImpl->isUnicode)
	{
		return false;
	}

	auto r = strcmp(sz, internalImpl->ansi.buffer);
	return r == 0;
}

bool string::operator==(const wchar_t* sz) const
{
	if (!internalImpl->isUnicode)
	{
		return false;
	}
	return false;
}

bool string::operator==(const string str) const
{
	if (internalImpl->isUnicode != str.internalImpl->isUnicode)
		return false;

	if (internalImpl->isUnicode)
		return false;

	if (str.internalImpl == internalImpl)
		return true;

	return operator==(str.internalImpl->ansi.buffer);
}

bool string::operator!=(const char* sz) const
{
	return !operator==(sz);
}

bool string::operator!=(const wchar_t* sz) const
{
	return !operator==(sz);
}

bool string::operator!=(const string str) const
{
	return !operator==(str);
}


// sugur
string& string::operator << (const char* sz)
{
	*this += sz;
	return *this;
}

string& string::operator << (const wchar_t* sz)
{
	*this += sz;
	return *this;
}

string& string::operator << (const string str)
{
	*this += str;
	return *this;
}

void string::print(std::ostream& out) const
{
	if (!internalImpl || length() <= 0)
		return;

	if (internalImpl->isUnicode)
	{
		auto ansi = Utf16ToAnsi(internalImpl->unicode);
		if (!ansi.buffer)
			return;

		if (ansi.len < ansi.maxLength)
			ansi.buffer[ansi.len] = 0;

		out << ansi.buffer;
		FreeStringBuff(ansi);
	}
	else
	{
		if (internalImpl->ansi.len < internalImpl->ansi.maxLength)
		{
			internalImpl->ansi.buffer[internalImpl->ansi.len] = 0;
			out << internalImpl->ansi.buffer;
		}
		else
		{
			char* buff = new char[internalImpl->ansi.len + 1];
			memcpy_s(buff, internalImpl->ansi.len + 1, internalImpl->ansi.buffer, internalImpl->ansi.len);
			buff[internalImpl->ansi.len] = 0;
			out << buff;
			delete[] buff;
		}
	}
}


//---------------------------------------------------------------------
void string::init(bool useUnicode, int initBuffLen)
{
	internalImpl = new UnderGround();
	internalImpl->isUnicode = useUnicode;
	internalImpl->refCount = 1;

	if (useUnicode)
	{
		internalImpl->unicode.len = 0;
		internalImpl->unicode.maxLength = 0;
		internalImpl->unicode.buffer = nullptr;

		if (initBuffLen > 0)
		{
			internalImpl->unicode.buffer = new wchar_t[initBuffLen];
			internalImpl->unicode.maxLength = initBuffLen;
		}
	}
	else
	{
		internalImpl->ansi.len = 0;
		internalImpl->ansi.maxLength = 0;
		internalImpl->ansi.buffer = nullptr;

		if (initBuffLen > 0)
		{
			internalImpl->ansi.buffer = new char[initBuffLen];
			internalImpl->ansi.maxLength = initBuffLen;
		}
	}
}

inline
int string::getUndergroundCapacity()
{
	if (internalImpl->isUnicode)
		return internalImpl->unicode.maxLength;
	else
		return internalImpl->ansi.maxLength;
}

inline
void string::copyUnderBuff(UnderGround* dest, UnderGround* source)
{
	if (dest->isUnicode)
		memcpy_s(dest->unicode.buffer, dest->unicode.maxLength * sizeof(wchar_t), source->unicode.buffer, source->unicode.len * sizeof(wchar_t));
	else
		memcpy_s(dest->ansi.buffer, dest->ansi.maxLength, source->ansi.buffer, source->ansi.len);
}

void string::reallocUnderground(int capacity, bool useCopy)
{
	if (internalImpl->isUnicode)
	{
		auto newBuff = new wchar_t[capacity];
		if (useCopy)
			memcpy_s(newBuff, capacity * sizeof(wchar_t), internalImpl->unicode.buffer, internalImpl->unicode.len * sizeof(wchar_t));

		delete[] internalImpl->unicode.buffer;

		internalImpl->unicode.buffer = newBuff;
		internalImpl->unicode.maxLength = capacity;
	}
	else
	{
		auto newBuff = new char[capacity];
		if (useCopy)
			memcpy_s(newBuff, capacity, internalImpl->ansi.buffer, internalImpl->ansi.len);

		delete[] internalImpl->ansi.buffer;

		internalImpl->ansi.buffer = newBuff;
		internalImpl->ansi.maxLength = capacity;
	}
}

void string::leaveUnderground()
{
	auto ref = DecUndergroundRef(internalImpl);
	if (ref <= 0)
	{
		if (internalImpl->isUnicode)
		{
			if (internalImpl->unicode.buffer)
				delete[] internalImpl->unicode.buffer;
		}
		else if (internalImpl->ansi.buffer)
		{
			delete[] internalImpl->ansi.buffer;
		}

		delete internalImpl;
	}

	internalImpl = nullptr;
}

void string::writeCopy(int miniCapacity)
{
	if (!internalImpl)
	{
		init(USE_UNICODE, miniCapacity);
		return;
	}

	auto currentCapacity = getUndergroundCapacity();

	if (miniCapacity == 0)
		miniCapacity = currentCapacity;

	if (internalImpl->refCount == 1)
	{
		if (currentCapacity != miniCapacity)
		{
			if (currentCapacity < miniCapacity)
				reallocUnderground(miniCapacity, true);
			else if (currentCapacity > miniCapacity * 2)
				reallocUnderground(miniCapacity, false);
		}

		return;
	}

	auto oldInternal = internalImpl;

	init(oldInternal->isUnicode, miniCapacity);

	if (miniCapacity >= currentCapacity)
	{
		copyUnderBuff(internalImpl, oldInternal);
		if (internalImpl->isUnicode)
			internalImpl->unicode.len = oldInternal->unicode.len;
		else
			internalImpl->ansi.len = oldInternal->ansi.len;
	}

	DecUndergroundRef(oldInternal);
}

void string::writeInit()
{
	auto useUnicode = USE_UNICODE;
	if (internalImpl)
	{
		useUnicode = internalImpl->isUnicode;
		if (internalImpl->refCount == 1)
		{
			return;
		}

		leaveUnderground();
	}

	init(useUnicode);
}


#if defined(_DEBUG_) || defined(DEBUG) || defined(_DEBUG)
UnderGround* UnderGround::head = nullptr;
#endif
