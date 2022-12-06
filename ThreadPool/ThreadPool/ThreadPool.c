#include "ThreadPool.h"
#include <stdlib.h>
#include <stdio.h>
#include<string.h>
#define NUMBER 2
#include <unistd.h>

ThreadPool * ThreadPoolInit(int min, int max, int queueCapacity)
{
	ThreadPool * pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	if (pool == NULL)
	{
		printf("pool创建失败");
		return NULL;
	}

	//任务队列
	pool->queue = (TaskQueue*)malloc(sizeof(TaskQueue)*queueCapacity);
	if (pool->queue == NULL)
	{
		printf("任务队列创建失败");
		return NULL;
	}
	pool->queueCapacity = queueCapacity;
	pool->queueFront = pool->queueRear = 0;
	pool->queueSize = 0;

	//创建线程
	pool->workers = (pthread_t *)malloc(sizeof(pthread_t)*max);//***
	if (pool->workers == NULL)
	{
		printf("创建线程失败");
		return NULL;
	}
	memset(pool->workers, 0, sizeof(pthread_t)*max);//*******
	//线程池
	pthread_create(&pool->manager, NULL, manager, pool);
	for (int i = 0; i < min; i++)
	{
		pthread_create(&pool->workers[i], NULL, worker, pool);
	}
	pool->minNum = min;
	pool->maxNum = max;
	pool->killNum = 0;
	pool->liveNum = min;
	pool->workNum = 0;
	pool->shutdown = 0;

	if (pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
		pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
		pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
		pthread_cond_init(&pool->notFull, NULL) != 0)
	{
		printf("互斥量或信号创建失败");
		return NULL;
	}

	return pool;
}

void * manager(void * arg)
{
	ThreadPool * pool = (ThreadPool*)arg;
	while (!pool->shutdown)//******************
	{
		sleep(3);
		pthread_mutex_lock(&pool->mutexPool);
		int liveNum = pool->liveNum;
		int queueSize = pool->queueSize;
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_mutex_lock(&pool->mutexBusy);
		int workNum = pool->workNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		//添加线程
		if (liveNum < queueSize && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			for (int i = 0, count = 0;i < pool->maxNum && count < NUMBER &&
				liveNum < pool->maxNum;i++)
			{
				if (pool->workers[i] == 0)
				{
					pthread_create(&pool->workers[i], NULL, worker, pool);
					pool->liveNum++;
					count++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
		//删除线程
		if (workNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->killNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			for (int i = 0; i < NUMBER; i++)
			{
				pthread_cond_signal(&pool->notEmpty);//**********
			}
		}
	}
	return NULL;
}

void * worker(void * arg)
{
	ThreadPool *pool = (ThreadPool*)arg;
	while (1)
	{
		pthread_mutex_lock(&pool->mutexPool);
		while (pool->queueSize == 0 && !pool->shutdown)
		{
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

			if (pool->killNum > 0)
			{
				pool->killNum--;
				if (pool->liveNum > pool->minNum)//*********
				{
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					DeleteThread(pool);
				}
			}
		}
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexPool);
			DeleteThread(pool);
		}

		//取出任务
		TaskQueue queue;
		queue.function = pool->queue[pool->queueFront].function;
		queue.arg = pool->queue[pool->queueFront].arg;


		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		pthread_mutex_unlock(&pool->mutexPool);

		pthread_cond_signal(&pool->notFull);//**********************

		printf("线程%ld开始执行...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->workNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		//执行任务
		queue.function(queue.arg);
		free(queue.arg);//**************************
		queue.arg = NULL;//******************
		
		printf("线程%ld执行结束...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->workNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

void DeleteThread(ThreadPool * pool)
{
	pthread_t tid = pthread_self();

	for (int i = 0; i < pool->maxNum; i++)
	{
		if (pool->workers[i] == tid)
		{
			pool->workers[i] = 0;
			printf("线程%ld销毁成功...\n", tid);
			break;//*********
		}
	}
	pthread_exit(NULL);
}

void AddTaskQueue(ThreadPool * pool, void(*function)(void *arg), void *arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	//任务队列满了
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
	{
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	//往任务队列添加任务
	pool->queue[pool->queueRear].function = function;
	pool->queue[pool->queueRear].arg = arg;

	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;
	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
}

int DestoryThreadPool(ThreadPool * pool)
{
	if (pool == NULL)
		return -1;
	pool->shutdown = 1;
	//销毁任务队列
	if (pool->queue)//***************
	{
		free(pool->queue);
	}
	if (pool->workers)//************
	{
		free(pool->workers);
	}

	pool->queueCapacity = 0;
	pool->queueFront = pool->queueRear = 0;
	pool->queueSize = 0;

	//销毁线程池
	pthread_join(pool->manager, NULL);
	for (int i = 0; i < pool->liveNum;i++)//***************
	{
		pthread_cond_signal(&pool->notEmpty);//*************
	}
	pool->liveNum = 0;
	pool->killNum = 0;
	pool->maxNum = 0;
	pool->minNum = 0;
	pool->workNum = 0;

	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);
	free(pool);
	pool = NULL;
	return 0;
}