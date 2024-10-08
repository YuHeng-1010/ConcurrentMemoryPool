#define _CRT_SECURE_NO_WARNINGS 1

#include <atomic>
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

	std::cout << "new耗时:" << end1 - begin1 << "ms" << std::endl;
	std::cout << "ObjectPool耗时:" << end2 - begin2 << "ms" << std::endl;
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

	std::cout << p1 << std::endl;
	std::cout << p2 << std::endl;
	std::cout << p3 << std::endl;
	std::cout << p4 << std::endl;
	std::cout << p5 << std::endl;
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; ++i)
	{
		void* p1 = ConcurrentAlloc(6);
		std::cout << p1 << std::endl;
	}

	void* p2 = ConcurrentAlloc(8);
	std::cout << p2 << std::endl;
}

void TestAddressShift()
{
	PAGE_ID id1 = 2000;
	PAGE_ID id2 = 2001;
	char* p1 = (char*)(id1 << PAGE_SHIFT);
	char* p2 = (char*)(id2 << PAGE_SHIFT);
	while (p1 < p2)
	{
		std::cout << (void*)p1 << ":" << ((PAGE_ID)p1 >> PAGE_SHIFT) << std::endl;
		p1 += 8;
	}
}

void TestAllocAndFree()
{
	void* p1 = ConcurrentAlloc(6);
	ConcurrentFree(p1);
}

void MultiThreadAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* ptr = ConcurrentAlloc(1024);
		v.push_back(ptr);
	}

	for (auto e : v)
	{
		ConcurrentFree(e);
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
		ConcurrentFree(e);
	}
}

void TestMultiThread()
{
	std::thread t1(MultiThreadAlloc1);
	std::thread t2(MultiThreadAlloc2);

	t1.join();
	t2.join();
}

void BigAlloc()
{
	// 申请释放一次257KB内存，验证大块内存申请释放，调试观察向page cache切分和合并大块页的逻辑
	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);

	// 申请释放一次129*8KB内存，验证超大块内存申请释放，调试观察向堆申请释放内存逻辑
	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);
}

void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(16));
					//v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		   nworks, rounds, ntimes, malloc_costtime.load());
	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		   nworks, rounds, ntimes, free_costtime.load());
	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		   nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}

// 单轮次申请释放次数 线程数 轮次 每次申请的内存大小
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(16));
					//v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n",
		   nworks, rounds, ntimes, malloc_costtime.load());
	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",
		   nworks, rounds, ntimes, free_costtime.load());
	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",
		   nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}

void ContrastMallocAndConcurrentMalloc()
{
	size_t n = 10000;
	std::cout << "==========================================================" << std::endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	std::cout << std::endl << std::endl;
	BenchmarkMalloc(n, 4, 10);
	std::cout << "==========================================================" << std::endl;
}

void TestComplexEnvironment()
{
	for (int i = 0; i < 100; i++)
	{
		ContrastMallocAndConcurrentMalloc();
	}
}

int main()
{
	//TestObjectPool();

	//TestTLS();

	//TestConcurrentAlloc1();

	//TestConcurrentAlloc2();

	//TestAddressShift(); // 8*1024

	//TestAllocAndFree();

	//TestMultiThread();

	//ContrastMallocAndConcurrentMalloc();

	//BigAlloc();

	//std::cout << sizeof(ThreadCache);

	TestComplexEnvironment();


	return 0;
}