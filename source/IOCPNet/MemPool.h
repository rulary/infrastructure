
#pragma once

class MemPool
{
public:
	static void* Alloc(size_t size,bool isZeroInit = false);
	static void  Free(void* memPtr);

	static void  ReserveMinSize(size_t size);

};