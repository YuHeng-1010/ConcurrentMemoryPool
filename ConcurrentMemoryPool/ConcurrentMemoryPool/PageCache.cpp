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