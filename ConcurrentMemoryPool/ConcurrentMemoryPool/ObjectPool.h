#pragma once
#include "Common.h"

#ifdef _WIN32
	#include <windows.h>
#else
	// Linux��brk��mmap��ͷ�ļ�
#endif


template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;

		// �����пռ��������з��ص��ڴ�飬������ʹ��
		if (_freeList)
		{
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;
		}
		else
		{
			// ʣ��ռ䲻�����һ������ʱ�����¿����ռ�
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				// 8KBΪһҳȥ���������ڴ�
				_memory = (char*)SystemAlloc(_remainBytes >> 13); 
				if (_memory == nullptr)
					throw std::bad_alloc();
			}

			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		// replacement new����ʽ����T�Ĺ��캯��
		new(obj)T();

		return obj;
	}

	void Delete(T* obj)
	{
		// ��ʽ����T����������
		obj->~T();

		// ����пռ����������ͷ��
		*(void**)obj = _freeList;
		_freeList = *(void**)obj;
	}

private:
	char* _memory = nullptr;	// ָ������ϵͳ���뵫δ������û����ڴ�ָ��
	size_t _remainBytes = 0;	// ����ϵͳ���뵫δ������û���ʣ���ڴ��С
	void* _freeList = nullptr;	// ���пռ������ͷָ��
};