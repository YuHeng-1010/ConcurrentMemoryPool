#pragma once
#include "Common.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// 获取一个K页的span
	Span* NewSpan(size_t k);

private:
	PageCache()
	{}

	PageCache(const PageCache&) = delete;

	SpanList _spanLists[NPAGES];
	static PageCache _sInst;

public:
	std::mutex _pageMtx;
};