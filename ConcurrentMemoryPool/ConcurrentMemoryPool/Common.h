#pragma once

#include <iostream>
#include <vector>
#include <algorithm>

#include <time.h>
#include <assert.h>

#include <thread>
#include <mutex>

#ifdef _WIN32
	#include <Windows.h>
#else
	// Linux
#endif

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024;	// 向thread cache申请的最大内存数，超过则直接向page cache申请。
static const size_t NFREELIST = 208;		// 哈希桶的个数
static const size_t NPAGES = 129;			// 页数
static const size_t PAGE_SHIFT = 13;		// 2^13=8k

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#else
	// Linux
#endif

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

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

	void PushRange(void* begin, void* end)
	{
		NextObj(end) = _freeList;
		_freeList = begin;
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

	size_t& MaxSize()
	{
		return _maxSize;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
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

	// 一次thread cache从central cache里获取多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		size_t num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	// 计算一次向系统获取多少页
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = (num * size) >> PAGE_SHIFT;

		if (npage == 0)
			npage = 1;

		return npage;
	}
};


// 管理多个连续页大块内存跨度结构
struct Span
{
	PAGE_ID _pageId = 0; // 大块内存起始页的页号
	size_t  _n = 0;      // 页的数量

	Span* _next = nullptr;	// 双向链表的结构
	Span* _prev = nullptr;

	size_t _useCount = 0; // 切好小块内存，被分配给thread cache的计数
	void* _freeList = nullptr;  // 切好的小块内存的自由链表
};

// 带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	Span* PopFront()
	{
		Span* span = _head->_next;
		Erase(span);
		return span;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		// prev newspan pos
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	bool Empty()
	{
		return _head == _head->_next;
	}

private:
	Span* _head;
public:
	std::mutex _mtx; // 桶锁
};