#define _CRT_SECURE_NO_WARNINGS 1
#include "Common.h"
#include "ThreadCache.h"
#include "CentralCache.h"

// 申请和释放内存对象
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

	// 找对映射的自由链表桶，对象插入进入
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);
}

// 从中心缓存获取对象
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢开始反馈调节算法
	// 最开始不会向central cache要太多内存
	// 如果不要size的内存，则会使MaxSize不断增加，直到达到上限
	size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
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
		_freeLists[index].PushRange(NextObj(begin), end);
		return begin;
	}
}
