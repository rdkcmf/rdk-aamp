/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file AampScheduler.h
 * @brief Class to schedule commands for async execution
 */

#ifndef __AAMP_SCHEDULER_H__
#define __AAMP_SCHEDULER_H__

#include <functional>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <utility>
#include "AampLogManager.h"

#define AAMP_SCHEDULER_ID_MAX_VALUE 10000
#define AAMP_SCHEDULER_ID_DEFAULT 1		//ID ranges from DEFAULT to MAX
#define AAMP_SCHEDULER_ID_INVALID -1

typedef std::function<void (void *)> AsyncTask;

struct AsyncTaskObj
{
	AsyncTask mTask;
	void * mData;
	int mId;

	AsyncTaskObj(AsyncTask task, void *data, int id = AAMP_SCHEDULER_ID_INVALID) :
				mTask(task), mData(data), mId(id)
	{
	}

	AsyncTaskObj(const AsyncTaskObj &other) : mTask(other.mTask), mData(other.mData), mId(other.mId)
	{
	}

	AsyncTaskObj& operator=(const AsyncTaskObj& other)
	{
		mTask = other.mTask;
		mData = other.mData;
		mId = other.mId;
		return *this;
	}
};

/**
 * @brief Scheduler class for asynchronous operations
 */
class AampScheduler
{
public:
	/**
	 * @brief AampScheduler Constructor
	 */
	AampScheduler();

	AampScheduler(const AampScheduler&) = delete;
	AampScheduler& operator=(const AampScheduler&) = delete;
	/**
	 * @brief AampScheduler Destructor
	 */
	virtual ~AampScheduler();

	/**
	 * @brief To schedule a task to be executed later
	 *
	 * @param[in] obj - object to be scheduled
	 * @return int - scheduled task id
	 */
	int ScheduleTask(AsyncTaskObj obj);

	/**
	 * @brief To remove all scheduled tasks and prevent further tasks from scheduling
	 *
	 * @return void
	 */
	void RemoveAllTasks();

	/**
	 * @brief To remove a scheduled tasks with ID
	 *
	 * @param[in] id - ID of task to be removed
	 * @return bool true if removed, false otherwise
	 */
	bool RemoveTask(int id);

protected:
	/**
	 * @brief To start scheduler thread
	 *
	 * @return void
	 */
	void StartScheduler();

	/**
	 * @brief To stop scheduler and associated resources
	 *
	 * @return void
	 */
	void StopScheduler();

	/**
	 * @brief To acquire execution lock for synchronisation purposes
	 *
	 * @return void
	 */
	void AcquireExLock();

	/**
	 * @brief To release execution lock
	 *
	 * @return void
	 */
	void ReleaseExLock();

	/**
	 * @brief To enable scheduler to queue new tasks
	 *
	 * @return void
	 */
	void EnableScheduleTask();
	AampLogManager *mLogObj;
private:
	/**
	 * @brief Executes scheduled tasks - invoked by thread
	 *
	 * @return void
	 */
	void ExecuteAsyncTask();

	std::deque<AsyncTaskObj> mTaskQueue;	// Queue for storing scheduled tasks
	std::mutex mQMutex;			// Mutex for accessing mTaskQueue
	std::condition_variable mQCond;		// To notify when a task is queued in mTaskQueue
	bool mSchedulerRunning;			// Flag denotes if scheduler thread is running
	std::thread mSchedulerThread;		// Scheduler thread
	std::mutex mExMutex;			// Execution mutex for synchronization
	std::unique_lock<std::mutex> mExLock;	// Lock to be used by AcquireExLock and ReleaseExLock
	int mNextTaskId;			// counter that holds ID value of next task to be scheduled
	int mCurrentTaskId;			// ID of current executed task
	bool mLockOut;				// flag indicates if the queue is locked out or not
};

#endif /* __AAMP_SCHEDULER_H__ */
