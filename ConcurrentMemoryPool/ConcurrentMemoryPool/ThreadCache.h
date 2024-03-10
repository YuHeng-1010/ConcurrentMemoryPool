#pragma once
#include "Common.h"

class ThreadCache
{
public:
	// 申请和释放内存对象
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);

	// 当释放对象而链表过长时，将回收的内存还给central cache
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeLists[NFREELIST];
};

// TLS:thread local storage
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;