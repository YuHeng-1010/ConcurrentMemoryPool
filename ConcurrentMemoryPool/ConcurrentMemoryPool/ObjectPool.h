#pragma once
#include "Common.h"

#ifdef _WIN32
	#include <windows.h>
#else
	// Linux下brk、mmap的头文件
#endif


template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;

		// 若空闲空间链表里有返回的内存块，则优先使用
		if (_freeList)
		{
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;
		}
		else
		{
			// 剩余空间不够存放一个对象时，重新开块大空间
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				// 8KB为一页去堆上申请内存
				_memory = (char*)SystemAlloc(_remainBytes >> 13); 
				if (_memory == nullptr)
					throw std::bad_alloc();
			}

			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		// replacement new，显式调用T的构造函数
		new(obj)T();

		return obj;
	}

	void Delete(T* obj)
	{
		// 显式调用T的析构函数
		obj->~T();

		// 向空闲空间链表里进行头插
		*(void**)obj = _freeList;
		_freeList = *(void**)obj;
	}

private:
	char* _memory = nullptr;	// 指向已向系统申请但未分配给用户的内存指针
	size_t _remainBytes = 0;	// 已向系统申请但未分配给用户的剩余内存大小
	void* _freeList = nullptr;	// 空闲空间链表的头指针
};