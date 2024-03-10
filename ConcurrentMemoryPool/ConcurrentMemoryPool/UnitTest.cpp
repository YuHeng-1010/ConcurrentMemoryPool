#define _CRT_SECURE_NO_WARNINGS 1
#include "ObjectPool.h"
#include "Common.h"
#include "ConcurrentAlloc.h"

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{
	// 申请释放的轮次
	const size_t Rounds = 5;

	// 每轮申请释放多少次
	const size_t N = 100000;

	std::vector<TreeNode*> v1;
	v1.reserve(N);

	size_t begin1 = clock();
	for (size_t i = 0; i < Rounds; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			v1.push_back(new TreeNode);
		}
		for (int j = 0; j < N; ++j)
		{
			delete v1[j];
		}
		v1.clear();
	}
	size_t end1 = clock();

	////////////////////////////////////////////////
	////////////////////////////////////////////////
	////////////////////////////////////////////////

	std::vector<TreeNode*> v2;
	v2.reserve(N);

	ObjectPool<TreeNode> TNPool;
	size_t begin2 = clock();
	for (size_t i = 0; i < Rounds; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			v2.push_back(TNPool.New());
		}
		for (int j = 0; j < N; ++j)
		{
			TNPool.Delete(v2[j]);
		}
		v2.clear();
	}
	size_t end2 = clock();

	cout << "new耗时:" << end1 - begin1 << "ms" << endl;
	cout << "ObjectPool耗时:" << end2 - begin2 << "ms" << endl;
}

void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(100);
	}
}

void TestTLS()
{
	std::thread t1(Alloc1);
	t1.join();

	std::thread t2(Alloc2);
	t2.join();
}

void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; ++i)
	{
		void* p1 = ConcurrentAlloc(6);
		cout << p1 << endl;
	}

	void* p2 = ConcurrentAlloc(8);
	cout << p2 << endl;
}

void TestAddressShift()
{
	PAGE_ID id1 = 2000;
	PAGE_ID id2 = 2001;
	char* p1 = (char*)(id1 << PAGE_SHIFT);
	char* p2 = (char*)(id2 << PAGE_SHIFT);
	while (p1 < p2)
	{
		cout << (void*)p1 << ":" << ((PAGE_ID)p1 >> PAGE_SHIFT) << endl;
		p1 += 8;
	}
}

void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e, 6);
	}
}

void MultiThreadAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(16);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e, 16);
	}
}

void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);

	t1.join();
	t2.join();
}

int main()
{
	//TestObjectPool();

	//TestTLS();

	//TestConcurrentAlloc2();

	//TestAddressShift(); // 8*1024

	TestMultiThread();

	return 0;
}