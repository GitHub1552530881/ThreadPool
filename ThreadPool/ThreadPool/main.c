#include <stdio.h>
#include <stdlib.h>
#include "ThreadPool.h"
#include <unistd.h>

void function(void * arg)
{
	int num = *(int*)arg;
	printf("线程%ld正在工作，num = %ld\n", pthread_self(), num);
	sleep(1);
}

int main()
{
	//创建线程池
	ThreadPool * pool = ThreadPoolInit(3, 10, 100);
	for (int i = 0; i < 100;i++)
	{
		int *num = (int*)malloc(sizeof(int));
		*num = i + 100;
		AddTaskQueue(pool, function, num);
	}
	sleep(30);
	DestoryThreadPool(pool);
	return 0;
}