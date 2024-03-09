#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_sInst;

// ��ȡһ��kҳ��span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0 && k < NPAGES);

	// ����k��Ͱ������span
	if (!_spanLists[k].Empty())
	{
		return _spanLists[k].PopFront();
	}

	// �������Ͱ����û��span�����򻮷�
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			Span* kSpan = new Span;

			// ��nSpanͷkҳ�и�kSpan
			// �ٽ�ʣ��n-kҳ���¹ҵ�ӳ���λ��
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;

			_spanLists[nSpan->_n].PushFront(nSpan);

			return kSpan;
		}
	}

	// �ߵ������ʾ����Ͱ�ﶼû��span������ȥ�������128ҳspan
	Span* maxSpan = new Span;
	void* ptr = SystemAlloc(NPAGES - 1);
	maxSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	maxSpan->_n = NPAGES - 1;

	_spanLists[maxSpan->_n].PushFront(maxSpan);

	return NewSpan(k);
}