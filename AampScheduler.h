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

//#define __UNIT_TESTING__

#include <functional>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <utility>
#include "AampDefine.h"
#include "AampEvent.h"
#ifndef __UNIT_TESTING__
#include "AampConfig.h"
#include "AampLogManager.h"
#else
#include <unistd.h>
#define AAMPLOG_WARN(FORMAT, ...) { printf(FORMAT,##__VA_ARGS__);printf("\n");}
#define AAMPLOG_INFO(FORMAT, ...) { printf(FORMAT,##__VA_ARGS__);printf("\n");}
#define AAMPLOG_ERR(FORMAT, ...)  { printf(FORMAT,##__VA_ARGS__);printf("\n");}
#endif
#define AAMP_SCHEDULER_ID_MAX_VALUE INT_MAX  // 10000
#define AAMP_SCHEDULER_ID_DEFAULT 1		//ID ranges from DEFAULT to MAX


typedef std::function<void (void *)> AsyncTask;

struct AsyncTaskObj
{
	AsyncTask mTask;
	void * mData;
	int mId;
	std::string mTaskName;

	AsyncTaskObj(AsyncTask task, void *data, std::string tskName="", int id = AAMP_TASK_ID_INVALID) :
				mTask(task), mData(data), mId(id),mTaskName(tskName)
	{
	}

	AsyncTaskObj(const AsyncTaskObj &other) : mTask(other.mTask), mData(other.mData), mId(other.mId),mTaskName(other.mTaskName)
	{
	}

	AsyncTaskObj& operator=(const AsyncTaskObj& other)
	{
		mTask = other.mTask;
		mData = other.mData;
		mId = other.mId;
		mTaskName = other.mTaskName;
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
	void SuspendScheduler();

	/**
	 * @brief To release execution lock
	 *
	 * @return void
	 */
	void ResumeScheduler();

	/**
	 * @brief To enable scheduler to queue new tasks
	 *
	 * @return void
	 */
	void EnableScheduleTask();
	/**
	 * @brief To player state to Scheduler
	 *
	 * @return void
	 */
	void SetState(PrivAAMPState sstate);
	/**
	 *  @brief Set the logger instance for the Scheduler
	 *
	 *   @return void
	 */   
	void SetLogger(AampLogManager *logObj) { mLogObj = logObj;}
protected:
#ifndef __UNIT_TESTING__
	AampLogManager *mLogObj;
#else
    void *mLogObj;
#endif

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
	std::unique_lock<std::mutex> mExLock;	// Lock to be used by SuspendScheduler and ResumeScheduler
	int mNextTaskId;			// counter that holds ID value of next task to be scheduled
	int mCurrentTaskId;			// ID of current executed task
	bool mLockOut;				// flag indicates if the queue is locked out or not
	PrivAAMPState mState;		// Player State
};

#endif /* __AAMP_SCHEDULER_H__ */
