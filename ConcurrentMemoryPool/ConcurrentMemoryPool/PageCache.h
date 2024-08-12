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

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span回到给page cache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache()
	{}

	~PageCache()
	{}

	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;

public:
	std::mutex _pageMtx;
private:
	std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	SpanList _spanLists[NPAGES];
	static PageCache _sInst;
};