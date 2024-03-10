#pragma once
#include "Common.h"
#include "ThreadCache.h"

static void* ConcurrentAlloc(size_t size)
{
	// ÿ���߳�ͨ��TLS��������ר������̵߳�thread cache����
	if (pTLSThreadCache == nullptr)
	{
		pTLSThreadCache = new ThreadCache;
	}

	//test
	cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr, size_t size)
{
	assert(pTLSThreadCache);

	pTLSThreadCache->Deallocate(ptr, size);
}
