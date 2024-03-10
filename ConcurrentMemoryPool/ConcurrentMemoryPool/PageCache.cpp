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

			// �洢nSpan����λҳ�Ÿ�nSpanӳ�䣬����page cache�����ڴ�ʱ
			// ���еĺϲ�����
			_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;

			// ����id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				_idSpanMap[kSpan->_pageId + i] = kSpan;
			}

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

// ��ȡ�Ӷ���span��ӳ��
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

// �ͷſ���span�ص���page cache�����ϲ����ڵ�span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ��spanǰ���ҳ�����Խ��кϲ��������ڴ���Ƭ����
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		auto ret = _idSpanMap.find(prevId);

		// ǰ���ҳ��û�У������кϲ�
		if (ret == _idSpanMap.end())
		{
			break;
		}

		// ǰ������ҳ��span��ʹ�ã������кϲ�
		Span* prevSpan = ret->second;
		if (prevSpan->_isUse == true)
		{
			break;
		}

		// �ϲ�������128ҳ��spanû�취���������кϲ�
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		delete prevSpan;
	}

	// ���ϲ�
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