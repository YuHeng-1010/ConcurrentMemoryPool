#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// 向page cache申请一个非空span
Span* CentralCache::GetOneSpan(SpanList& spanlist, size_t size)
{
	// 查看当前的spanlist中是否有还有未分配对象的span
	Span* it = spanlist.Begin();
	while (it != spanlist.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}

	// 先把central cache的桶锁解掉，保证其他线程释放内存对象回来，不会阻塞
	spanlist._mtx.unlock();

	// 走到这里表示spanlist没有可以分配对象的span，接下来向page cache申请
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	PageCache::GetInstance()->_pageMtx.unlock();

	// 计算span的大块内存的起始地址和大小（单位为字节）
	char* begin = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = begin + bytes;

	// 把大块内存切成自由链表链接起来
	// 先切一块下来去做头，方便尾插
	span->_freeList = begin;
	begin += size;
	void* tail = span->_freeList;
	int i = 1;
	while (begin < end)
	{
		++i;
		NextObj(tail) = begin;
		tail = NextObj(tail); // tail = start;
		begin += size;
	}

	// 切好span以后，需要把span挂到桶里面去的时候，再加锁
	spanlist._mtx.lock();
	spanlist.PushFront(span);

	return span;
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
	span->_useCount += actualNum;

	_spanLists[index]._mtx.unlock();

	return actualNum;
}