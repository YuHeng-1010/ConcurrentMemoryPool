#pragma once
#include "Common.h"

// ����ģʽ������
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// ��page cache����һ���ǿ�span
	Span* GetOneSpan(SpanList& spanlist, size_t size);

	// ��central cache��ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& begin, void*& end, size_t batchNum, size_t size);

private:
	SpanList _spanLists[NFREELIST];

private:
	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;
};