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

	// ��һ�������Ķ����ͷŸ�span
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;

private:
	SpanList _spanLists[NFREELIST];
	static CentralCache _sInst;
};