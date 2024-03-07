#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <time.h>
#include <assert.h>

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024; // 向thread cache申请的最大内存数，超过则直接向page cache申请。
static const size_t NFREELIST = 208; // 哈希桶的个数

// 计算下一个结点的地址
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);

		NextObj(obj) = _freeList;
		_freeList = *(void**)obj;
	}

	void* Pop()
	{
		assert(_freeList);

		void* obj = _freeList;
		_freeList = NextObj(_freeList);

		return obj;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}
private:
	void* _freeList = nullptr;
};

// 计算对象大小的对齐映射规则
class SizeClass
{
public:
	// 整体控制在最多11%左右的内碎片浪费
	// 
	// 申请的内存数              对齐数			哈希桶分区            
	// [1,128]					8byte对齐	    freelist[0,16)
	// [128+1,1024]				16byte对齐	    freelist[16,72)
	// [1024+1,8*1024]			128byte对齐	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐    freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐  freelist[184,208)

	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return ((bytes + alignNum - 1) & ~(alignNum - 1));
	}

	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	static inline size_t Index(size_t size)
	{
		assert(size <= MAX_BYTES);

		// 每个区间的哈希桶数
		static int group_array[4] = { 16, 56, 56, 56 };

		if (size <= 128)
		{
			return _Index(size, 3);
		}
		else if (size <= 1024)
		{
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024)
		{
			return _Index(size - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (size <= 64 * 1024)
		{
			return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (size <= 256 * 1024)
		{
			return _Index(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}
};