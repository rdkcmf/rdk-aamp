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
 * @file AampScheduler.cpp
 * @brief Class to schedule commands for async execution
 */

#include "AampScheduler.h"
#include "AampConfig.h"

/**
 * @brief AampScheduler Constructor
 */
AampScheduler::AampScheduler() : mTaskQueue(), mQMutex(), mQCond(),
	mSchedulerRunning(false), mSchedulerThread(), mExMutex(),
	mExLock(mExMutex, std::defer_lock), mNextTaskId(AAMP_SCHEDULER_ID_DEFAULT),
	mCurrentTaskId(AAMP_SCHEDULER_ID_INVALID), mLockOut(false),
	mLogObj(NULL)
{
}

/**
 * @brief AampScheduler Destructor
 */
AampScheduler::~AampScheduler()
{
	if (mSchedulerRunning)
	{
		StopScheduler();
	}
}

/**
 * @brief To start scheduler thread
 *
 * @return void
 */
void AampScheduler::StartScheduler()
{
	//Turn on thread for processing async operations
	mSchedulerRunning = true;
	mSchedulerThread = std::thread(std::bind(&AampScheduler::ExecuteAsyncTask, this));
}

/**
 * @brief To schedule a task to be executed later
 *
 * @param[in] obj - object to be scheduled
 * @return void
 */
int AampScheduler::ScheduleTask(AsyncTaskObj obj)
{
	int id = AAMP_SCHEDULER_ID_INVALID;
	if (mSchedulerRunning)
	{
		std::lock_guard<std::mutex>lock(mQMutex);
		if (!mLockOut)
		{
			id = mNextTaskId++;
			// Upper limit check
			if (mNextTaskId > AAMP_SCHEDULER_ID_MAX_VALUE)
			{
				mNextTaskId = AAMP_SCHEDULER_ID_DEFAULT;
			}
			obj.mId = id;
			mTaskQueue.push_back(obj);
			mQCond.notify_one();
		}
		else
		{
			// Operation is skipped here, this might happen due to race conditions during normal operation, hence setting as info log
			AAMPLOG_INFO("Warning: Attempting to schedule a task when scheduler is locked out, skipping operation!!");
		}
	}
	else
	{
		AAMPLOG_ERR("Attempting to schedule a task when scheduler is not running, undefined behavior!");
	}
	return id;
}

/**
 * @brief Executes scheduled tasks - invoked by thread
 *
 * @return void
 */
void AampScheduler::ExecuteAsyncTask()
{
	std::unique_lock<std::mutex>lock(mQMutex);
	while (mSchedulerRunning)
	{
		if (mTaskQueue.empty())
		{
			AAMPLOG_WARN("Waiting for any functions to be queued!!");
			mQCond.wait(lock);
		}
		else
		{
			AAMPLOG_TRACE("Found entry in function queue!!");
			AsyncTaskObj obj = mTaskQueue.front();
			mTaskQueue.pop_front();
			if (obj.mId != AAMP_SCHEDULER_ID_INVALID)
			{
				mCurrentTaskId = obj.mId;
			}
			else
			{
				AAMPLOG_ERR("Scheduler found a task with invalid ID, skip task!");
				continue;
			}

			//Unlock so that new entries can be added to queue while function executes
			lock.unlock();

			{
				//Take execution lock
				std::lock_guard<std::mutex>lock(mExMutex);
				//Execute function
				obj.mTask(obj.mData);
			}

			lock.lock();
		}
	}
}

/**
 * @brief To remove all scheduled tasks and prevent further tasks from scheduling
 *
 * @return void
 */
void AampScheduler::RemoveAllTasks()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	if (!mTaskQueue.empty())
	{
		AAMPLOG_WARN("Clearing up %d entries from mFuncQueue", mTaskQueue.size());
		mTaskQueue.clear();
	}
	// A cleanup process is in progress, we should temporarily disable any new tasks from getting scheduled.
	mLockOut = true;
}

/**
 * @brief To stop scheduler and associated resources
 *
 * @return void
 */
void AampScheduler::StopScheduler()
{
	// Clean up things in queue
	mSchedulerRunning = false;
	// mLockOut will be set to true, preventing anyother tasks from getting scheduled
	RemoveAllTasks();
	mQCond.notify_one();
	mSchedulerThread.join();
}

/**
 * @brief To acquire execution lock for synchronisation purposes
 *
 * @return void
 */
void AampScheduler::AcquireExLock()
{
	mExLock.lock();
}

/**
 * @brief To remove all scheduled tasks
 *
 * @return void
 */
void AampScheduler::ReleaseExLock()
{
	mExLock.unlock();
}

/**
 * @brief To remove a scheduled tasks with ID
 *
 * @param[in] id - ID of task to be removed
 * @return bool true if removed, false otherwise
 */
bool AampScheduler::RemoveTask(int id)
{
	bool ret = false;
	std::lock_guard<std::mutex>lock(mQMutex);
	// Make sure its not currently executing/executed task
	if (id != AAMP_SCHEDULER_ID_INVALID && mCurrentTaskId != id)
	{
		for (auto it = mTaskQueue.begin(); it != mTaskQueue.end(); )
		{
			if (it->mId == id)
			{
				mTaskQueue.erase(it);
				ret = true;
				break;
			}
			else
			{
				it++;
			}
		}
	}
	return ret;
}

/**
 * @brief To enable scheduler to queue new tasks
 *
 * @return void
 */
void AampScheduler::EnableScheduleTask()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
}
