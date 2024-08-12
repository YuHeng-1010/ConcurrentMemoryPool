#pragma once
#include "Common.h"

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
	std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	SpanList _spanLists[NPAGES];
	static PageCache _sInst;
};