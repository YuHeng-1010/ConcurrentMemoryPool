#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"

CentralCache CentralCache::_sInst;

// 向page cache申请一个非空span
Span* CentralCache::GetOneSpan(SpanList& spanlist, size_t size)
{
	// ...
	return nullptr;
}

// 从central cache获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& begin, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	// 从span中获取batchNum个对象
	// 不够batchNum个，则有多少拿多少
	end = begin = span->_freeList;
	size_t i = 0;
	size_t actualNum = 1;
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;

	_spanLists[index]._mtx.unlock();

	return actualNum;
}