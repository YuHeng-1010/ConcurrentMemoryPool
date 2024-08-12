#define _CRT_SECURE_NO_WARNINGS 1
#include "Common.h"
#include "ThreadCache.h"
#include "CentralCache.h"

// ������ͷ��ڴ����
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t alighSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	// index�Ź�ϣͰ��Ϊ��
	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
	// index�Ź�ϣͰΪ��
	else
	{
		return FetchFromCentralCache(index, alighSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// �Ҷ�ӳ�����������Ͱ������������
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	// �������ȴ���һ������������ڴ�ʱ�Ϳ�ʼ��һ��list��central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	void* begin = nullptr;
	void* end = nullptr;
	list.PopRange(begin, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(begin, size);
}

// �����Ļ����ȡ����
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ����ʼ���������㷨
	// �ʼ������central cacheҪ̫���ڴ�
	// �����Ҫsize��С���ڴ棬���ʹMaxSize�������ӣ�ֱ���ﵽ����
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (batchNum == _freeLists[index].MaxSize())
		_freeLists[index].MaxSize() += 1;

	void* begin = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(begin, end, batchNum, size);
	assert(actualNum >= 1);

	if (actualNum == 1)
	{
		assert(begin == end);
		return begin;
	}
	else
	{
		_freeLists[index].PushRange(NextObj(begin), end, actualNum - 1);
		return begin;
	}
}
