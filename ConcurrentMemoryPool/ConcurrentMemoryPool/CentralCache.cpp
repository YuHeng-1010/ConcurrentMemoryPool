#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

// ��page cache����һ���ǿ�span
Span* CentralCache::GetOneSpan(SpanList& spanlist, size_t size)
{
	// �鿴��ǰ��spanlist���Ƿ��л���δ��������span
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

	// �Ȱ�central cache��Ͱ���������֤�����߳��ͷ��ڴ�����������������
	spanlist._mtx.unlock();

	// �ߵ������ʾspanlistû�п��Է�������span����������page cache����
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	PageCache::GetInstance()->_pageMtx.unlock();

	// ����span�Ĵ���ڴ����ʼ��ַ�ʹ�С����λΪ�ֽڣ�
	char* begin = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = begin + bytes;

	// �Ѵ���ڴ��г�����������������
	// ����һ������ȥ��ͷ������β��
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

	// �к�span�Ժ���Ҫ��span�ҵ�Ͱ����ȥ��ʱ���ټ���
	spanlist._mtx.lock();
	spanlist.PushFront(span);

	return span;
}

// ��central cache��ȡһ�������Ķ����thread cache
size_t CentralCache::FetchRangeObj(void*& begin, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	// ��span�л�ȡbatchNum������
	// ����batchNum�������ж����ö���
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

		// ˵��span���зֳ�ȥ������С���ڴ涼������
		// ���span�Ϳ����ٻ��ո�page cache��pagecache�����ٳ���ȥ��ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// �ͷ�span��page cacheʱ��ʹ��page cache�����Ϳ�����
			// ��ʱ��Ͱ�����
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