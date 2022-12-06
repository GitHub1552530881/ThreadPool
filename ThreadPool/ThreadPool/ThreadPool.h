#pragma once
#include <pthread.h>

typedef struct TaskQueue
{
	void(*function)(void * arg);
	void *arg;
}TaskQueue;

//线程池
typedef struct ThreadPool
{
	//任务队列
	TaskQueue *queue;
	int queueSize;
	int queueCapacity;
	int queueFront;
	int queueRear;

	//线程池
	pthread_t manager;
	pthread_t *workers;
	int maxNum;
	int minNum;
	int liveNum;
	int workNum;
	int killNum;
	int shutdown;

	pthread_mutex_t mutexPool;
	pthread_mutex_t mutexBusy;
	pthread_cond_t notFull;
	pthread_cond_t notEmpty;
}ThreadPool;

ThreadPool * ThreadPoolInit(int min, int max, int queueCapacity);
void * manager(void * arg);
void * worker(void * arg);

void DeleteThread(ThreadPool * pool);

void AddTaskQueue(ThreadPool * pool, void(*function)(void *arg), void *arg);

int DestoryThreadPool(ThreadPool * pool);
