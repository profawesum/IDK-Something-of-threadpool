#include "ThreadPool.h"
#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include <iostream>
#include <stdio.h>
#define Max_Threads 64

//Pointer to Threads
SDL_Thread *threads[Max_Threads];

//Mutex for thread safe sync no racing
SDL_mutex *queueMutex;
SDL_mutex *workerMutex;
SDL_mutex *mutexBarrier;

//condition variables
SDL_cond *condHaveWork;
SDL_cond *condBarrier;

//used to show that ThreadPool exsists somewhere
//external link
extern ThreadPool poolThread;

//This main function, while thread pool alive (detroy not called)
//this function monitor task queque, to process it

static int worker(void *param){

	while (1){

		//Try to get Task
		Task *task = NULL;
		SDL_LockMutex(queueMutex);
		//have task in queque
		int num = poolThread.workQueque.size();
		if (num > 0){
			//task = the front of the work queue
			task = poolThread.workQueque.front();
			//remove the front of the work queue
			poolThread.workQueque.pop();
		}
		//unlock the queue mutex
		SDL_UnlockMutex(queueMutex);

		// if the thread has a task then do it
		if (task != NULL){

			task->DoWork();

		}
		//if it has no task then idle
		if (num == 0){

			SDL_LockMutex(workerMutex);
			//get the condition to wake up
			SDL_CondWait(condHaveWork, workerMutex);
			//unlock the mutex
			SDL_UnlockMutex(workerMutex);
		}
	}
	return 0;
}
//constructor
ThreadPool::ThreadPool(){

}

//initialise the threadpool
void ThreadPool::init(int _numThreads){

	//create mutexes
	queueMutex = SDL_CreateMutex();
	workerMutex = SDL_CreateMutex();
	mutexBarrier = SDL_CreateMutex();
	//create condition variables
	condHaveWork = SDL_CreateCond();
	condBarrier = SDL_CreateCond();

	//setup number of threads
	numThreads = _numThreads;

	//initialise all threads
	for (int i = 0; i < numThreads; i++){

		//need a char for SDL_CreateThread
		char threadName[128];
		//Create thread
		threads[i] = SDL_CreateThread(worker, threadName, &poolThread);
		//if there are no threads
		if (NULL == threads[i]){

			std::cout << "failed to create thread" << std::endl;
		}

	}
}

//Destroy the thread pool
void ThreadPool::destroy(){

	//for each thread
	for (int i = 0; i < numThreads; i++){

		//Destroy Threads
		if (threads[i] != NULL) {
			SDL_DetachThread(threads[i]);
		}
	}

	//Destroy Mutexes
	SDL_DestroyMutex(queueMutex);
	SDL_DestroyMutex(workerMutex);
	SDL_DestroyMutex(mutexBarrier);

	//destroy condition variables
	SDL_DestroyCond(condBarrier);
	SDL_DestroyCond(condHaveWork);
}


//Enqueue work to pool
void ThreadPool::addWork(Task *task){

	//Enqueue work
	SDL_LockMutex(queueMutex);
	//push the task to the work queue
	workQueque.push(task);
	//size = the workQueue size
	int size = workQueque.size();
	//unlock the mutex
	SDL_UnlockMutex(queueMutex);

	//set condition to havework so the wait will trigger as having work
	SDL_LockMutex(workerMutex);
	SDL_CondSignal(condHaveWork);
	SDL_UnlockMutex(workerMutex);

}

//destructor
ThreadPool::~ThreadPool(){

}