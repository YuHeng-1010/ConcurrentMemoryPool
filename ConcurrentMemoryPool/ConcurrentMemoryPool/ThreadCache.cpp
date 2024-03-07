#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"

// ������ͷ��ڴ����
void* ThreadCache::Allocate(size_t size)
{
	assert(size < MAX_BYTES);
	size_t alighSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
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
}

// �����Ļ����ȡ����
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	return nullptr;
}
