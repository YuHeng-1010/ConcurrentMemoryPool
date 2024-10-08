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
	span->_isUse = true;
	span->_objSize = size;	
	PageCache::GetInstance()->_pageMtx.unlock();

	// 对获取到的span进行划分，走到这里其他线程获取不到span，所以不用加锁

	// 计算span的大块内存的起始地址和大小（单位为字节）
	char* begin = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = begin + bytes;

	// 把大块内存切成自由链表链接起来
	// 先切一块下来去做头，方便尾插
	span->_freeList = begin;
	begin += size;
	void* tail = span->_freeList;
	//int i = 1;
	while (begin < end)
	{
		//++i;
		NextObj(tail) = begin;
		tail = NextObj(tail); // tail = start;
		begin += size;
	}
	NextObj(tail) = nullptr;
	
	//Debug
	int testi = 0;
	void* cur = span->_freeList;
	while (cur)
	{
		cur = NextObj(cur);
		testi++;
	}
	if (bytes / size != testi)
	{
		int x = 0;
	}
	//Debug end

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

	//Debug
	int testi = 0;
	void* cur = begin;
	while (cur)
	{
		cur = NextObj(cur);
		testi++;
	}
	if (actualNum != testi)
	{
		int x = 0;
	}
	//Debug end

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

// 将begin开始的size大小内存还给central cache里的span
void CentralCache::ReleaseListToSpans(void* begin, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (begin)
	{
		void* next = NextObj(begin);

		Span* span = PageCache::GetInstance()->MapObjectToSpan(begin);
		NextObj(begin) = span->_freeList;
		span->_freeList = begin;
		span->_useCount--;

		// 说明span的切分出去的所有小块内存都回来了
		// 这个span就可以再回收给page cache，pagecache可以再尝试去做前后页的合并
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// 释放span给page cache时，使用page cache的锁就可以了
			// 这时把桶锁解掉
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		begin = next;
	}

	_spanLists[index]._mtx.unlock();
}