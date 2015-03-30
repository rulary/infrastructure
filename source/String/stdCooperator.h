#ifndef _STDCOOPERATOR_H_
#define _STDCOOPERATOR_H_

// sugur for std::io
#include "String.h"
#include <iostream>

std::ostream& operator<< (std::ostream& io, const StringEx::string& str)
{
	str.print(io);
	return io;
}

#endif	//_STDCOOPERATOR_H_