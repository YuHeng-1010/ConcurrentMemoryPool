#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
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

static const size_t MAX_BYTES = 256 * 1024;	// ��thread cache���������ڴ�����������ֱ����page cache����
static const size_t NFREELIST = 208;		// ��ϣͰ�ĸ���
static const size_t NPAGES = 129;			// ҳ��
static const size_t PAGE_SHIFT = 13;		// 2^13=8k

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#else
	// Linux
#endif

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

// ������һ�����ĵ�ַ
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
		_freeList = obj;
		++_size;
	}

	void PushRange(void* begin, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = begin;
		_size += n;
	}

	void* Pop()
	{
		assert(_freeList);

		void* obj = _freeList;
		_freeList = NextObj(_freeList);
		--_size;

		return obj;
	}
	
	void PopRange(void*& begin, void*& end, size_t n)
	{
		assert(n <= _size);
		begin = _freeList;
		end = begin;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}

		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};

// ��������С�Ķ���ӳ�����
class SizeClass
{
public:
	// ���� 129				byte�ڴ棬�˷� 15 / (128 + 16)		= 10.42% �ڴ�
	// ���� 1025				byte�ڴ棬�˷� 127 / (1024 + 128)	= 11.02% �ڴ�
	// ���� 8 * 1024 + 1		byte�ڴ棬�˷� 1023 / (8096 + 1024)	= 11.22% �ڴ�
	// ���� 64 * 1024 + 1	byte�ڴ棬�˷� 8095 / (65536 + 8096)	= 10.99% �ڴ�
	// ������������ 11% ���ҵ�����Ƭ�˷�
	// 
	// ������ڴ���              ������			��ϣͰ����            
	// [1,128]					8byte����	    freelist[0,16)
	// [128+1,1024]				16byte����	    freelist[16,72)
	// [1024+1,8*1024]			128byte����	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����    freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte����  freelist[184,208)

	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return ((bytes + alignNum - 1) & ~(alignNum - 1));
	}

	// �� size ���ж��룬����RoundUp(128) = 128, RoundUp(129) = 144,
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

	// ���� size ��Ӧ�Ĺ�ϣͰ�±�
	static inline size_t Index(size_t size)
	{
		assert(size <= MAX_BYTES);

		// ÿ������Ĺ�ϣͰ��
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

	// thread cacheһ�δ�central cache���ȡ���ٸ�����
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

	// ����һ����ϵͳ��ȡ����ҳ
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = (num * size) >> PAGE_SHIFT;

		if (npage == 0)
			npage = 1;

		return npage;
	}
};


// ����������ҳ����ڴ��Ƚṹ
struct Span
{
	PAGE_ID _pageId = 0;		// ����ڴ���ʼҳ��ҳ��
	size_t  _n = 0;				// ҳ������

	Span* _next = nullptr;		// ˫������Ľṹ
	Span* _prev = nullptr;

	size_t _useCount = 0;		// �к�С���ڴ棬�������thread cache�ļ���
	void* _freeList = nullptr;  // �кõ�С���ڴ����������

	bool _isUse = false;		// �Ƿ��ڱ�ʹ��
};

// ��ͷ˫��ѭ������
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
	std::mutex _mtx; // Ͱ��
};