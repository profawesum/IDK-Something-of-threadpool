#pragma once

#include <queue>
typedef enum TaskState {
	Initialized,
	Running,
	Finished
};
class Task
{
public:

	virtual void *DoWork() = 0;

};

class ThreadPool
{
public:
	// Default constructor
	ThreadPool();

	// Initialize with number of Threads
	void init(int _numThreads);

	// Destroy the pool
	void destroy();

	//Add task to Queque
	void addWork(Task *task);

	// Default destructor
	virtual ~ThreadPool();
	int numThreads;
	std::queue<Task*> workQueque;
};

extern ThreadPool poolThread;