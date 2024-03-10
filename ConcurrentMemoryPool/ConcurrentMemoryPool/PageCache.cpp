#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_sInst;

// 获取一个k页的span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0 && k < NPAGES);

	// 检查第k个桶里有无span
	if (!_spanLists[k].Empty())
	{
		return _spanLists[k].PopFront();
	}

	// 检查后面的桶里有没有span，有则划分
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = new Span;

			// 将nSpan头k页切给kSpan
			// 再将剩下n-k页重新挂到映射的位置
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;

			_spanLists[nSpan->_n].PushFront(nSpan);

			// 存储nSpan的首位页号跟nSpan映射，方便page cache回收内存时
			// 进行的合并查找
			_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;

			// 建立id和span的映射，方便central cache回收小块内存时，查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				_idSpanMap[kSpan->_pageId + i] = kSpan;
			}

			return kSpan;
		}
	}

	// 走到这里表示所有桶里都没有span，于是去向堆申请128页span
	Span* maxSpan = new Span;
	void* ptr = SystemAlloc(NPAGES - 1);
	maxSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	maxSpan->_n = NPAGES - 1;

	_spanLists[maxSpan->_n].PushFront(maxSpan);

	return NewSpan(k);
}

// 获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end())
	{
		return ret->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

// 释放空闲span回到给page cache，并合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 对span前后的页，尝试进行合并，缓解内存碎片问题
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		auto ret = _idSpanMap.find(prevId);

		// 前面的页号没有，不进行合并
		if (ret == _idSpanMap.end())
		{
			break;
		}

		// 前面相邻页的span在使用，不进行合并
		Span* prevSpan = ret->second;
		if (prevSpan->_isUse == true)
		{
			break;
		}

		// 合并出超过128页的span没办法管理，不进行合并
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		delete prevSpan;
	}

	// 向后合并
	while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = ret->second;
		if (nextSpan->_isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		delete nextSpan;
	}

	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;
}