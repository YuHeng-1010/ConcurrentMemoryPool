#pragma once
#include "Common.h"

// 单例模式饿汉版
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// 向page cache申请一个非空span
	Span* GetOneSpan(SpanList& spanlist, size_t size);

	// 从central cache获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& begin, void*& end, size_t batchNum, size_t size);

private:
	SpanList _spanLists[NFREELIST];

private:
	CentralCache()
	{}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;
};