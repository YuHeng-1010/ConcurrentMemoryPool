#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// ��ȡһ��Kҳ��span
	Span* NewSpan(size_t k);

	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص���page cache�����ϲ����ڵ�span
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
	SpanList _spanLists[NPAGES];

	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//std::map<PAGE_ID, Span*> _idSpanMap;	
	TCMalloc_PageMap2<32 - PAGE_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool;

	static PageCache _sInst;
};