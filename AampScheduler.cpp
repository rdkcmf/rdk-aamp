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
/**
 * @brief AampScheduler Constructor
 */
AampScheduler::AampScheduler() : mTaskQueue(), mQMutex(), mQCond(),
	mSchedulerRunning(false), mSchedulerThread(), mExMutex(),
	mExLock(mExMutex, std::defer_lock), mNextTaskId(AAMP_SCHEDULER_ID_DEFAULT),
	mCurrentTaskId(AAMP_TASK_ID_INVALID), mLockOut(false),
	mLogObj(NULL),mState(eSTATE_IDLE)
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
	mSchedulerThread = std::thread(std::bind(&AampScheduler::ExecuteAsyncTask, this));
	mSchedulerRunning = true;
	AAMPLOG_WARN("Started Async Worker Thread");
}

/**
 * @brief To schedule a task to be executed later
 *
 * @param[in] obj - object to be scheduled
 * @return void
 */
int AampScheduler::ScheduleTask(AsyncTaskObj obj)
{
	int id = AAMP_TASK_ID_INVALID;
	if (mSchedulerRunning)
	{

		if( mState == eSTATE_ERROR || mState == eSTATE_RELEASED)
			return id;

		std::lock_guard<std::mutex>lock(mQMutex);
		if (!mLockOut)
		{
			id = mNextTaskId++;
			// Upper limit check
			if (mNextTaskId >= AAMP_SCHEDULER_ID_MAX_VALUE)
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
			AAMPLOG_INFO("Warning: Attempting to schedule a task when scheduler is locked out, skipping operation %s!!", obj.mTaskName.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("Attempting to schedule a task when scheduler is not running, undefined behavior, task ignored:%s",obj.mTaskName.c_str());
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
			if (obj.mId != AAMP_TASK_ID_INVALID)
			{
				mCurrentTaskId = obj.mId;
				AAMPLOG_WARN("Found entry in function queue!!, task:%s. State:%d",obj.mTaskName.c_str(),mState);
				if( mState == eSTATE_ERROR || mState == eSTATE_RELEASED)
					continue;
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
				AAMPLOG_WARN("SchedulerTask Execution:%s",obj.mTaskName.c_str());
				//Execute function
				obj.mTask(obj.mData);
			}

			lock.lock();
		}
	}
	AAMPLOG_WARN("Exited Async Worker Thread");
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
		AAMPLOG_WARN("Clearing up %zu entries from mFuncQueue", mTaskQueue.size());
		mTaskQueue.clear();
	}
}

/**
 * @brief To stop scheduler and associated resources
 *
 * @return void
 */
void AampScheduler::StopScheduler()
{
	AAMPLOG_WARN("Stopping Async Worker Thread");
	// Clean up things in queue
	mSchedulerRunning = false;
	// mLockOut will be set to true, preventing anyother tasks from getting scheduled
	RemoveAllTasks();
	mQCond.notify_one();
    if (mSchedulerThread.joinable())
        mSchedulerThread.join();
}

/**
 * @brief To acquire execution lock for synchronisation purposes
 *
 * @return void
 */
void AampScheduler::SuspendScheduler()
{
	AAMPLOG_WARN("Suspending Async Worker Thread");
	mExLock.lock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = true;
}

/**
 * @brief To remove all scheduled tasks
 *
 * @return void
 */
void AampScheduler::ResumeScheduler()
{
	AAMPLOG_WARN("Resuming Async Worker Thread");
	mExLock.unlock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
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
	if (id != AAMP_TASK_ID_INVALID && mCurrentTaskId != id)
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

/**
 * @brief To Set the state of the player
 *
 * @return void
 */
void AampScheduler::SetState(PrivAAMPState sstate)
{
	mState = sstate;
}

#ifdef __UNIT_TESTING__

void printTask(const char *str)
{
    AAMPLOG_WARN("Inside task function :%s\n",str);
    int seconds = rand() % 10 + 1;
    sleep(seconds);
}

void AddTask(AampScheduler &mScheduler, std::string str )
{
    mScheduler.ScheduleTask(AsyncTaskObj([str](void *data)
					{
						printTask(str.c_str());
					}, (void *) nullptr, __FUNCTION__));

}
void task1(void *inst) {
    std::string input="Task1";
    AampScheduler *mScheduler = (AampScheduler *)inst;
    for(int i=0;i<10;i++)
    {
        mScheduler->ScheduleTask(AsyncTaskObj([input](void *data)
					{
						printTask(input.c_str());
					}, (void *) nullptr, __FUNCTION__));

        int state = rand() % 13 + 1;
        mScheduler->SetState((PrivAAMPState)state);
        AAMPLOG_WARN("setting state:%d",state);
    }
}

void task2(void *inst) {
    std::string input="Task2";
    AampScheduler *mScheduler = (AampScheduler *)inst;
    mScheduler->ScheduleTask(AsyncTaskObj([input](void *data)
					{
						printTask(input.c_str());
					}, (void *) nullptr, __FUNCTION__));

}
int main()
{
    AAMPLOG_WARN("Testing the Player Scheduler Class\n");
    AampScheduler mScheduler;
    srand(time(0));
    //Test 1: Simple start / stop scheduler
    //mScheduler.StartScheduler();
    //mScheduler.StopScheduler();
    // Test 2: start but no stop
    //mScheduler.StartScheduler();
    // Test 3 :
    //mScheduler.StartScheduler();
    //AddTask(mScheduler, "Test1");
    //sleep(10);
    //mScheduler.StopScheduler();
    // Test 4 :
    //mScheduler.StartScheduler();
    //mScheduler.SetState(eSTATE_RELEASED);
    //AddTask(mScheduler, "Test2");
    //sleep(10);
    //mScheduler.StopScheduler();
    // Test 5 :
    //mScheduler.StartScheduler();
    //AddTask(mScheduler, "Test2");
    //mScheduler.SetState(eSTATE_RELEASED);
    //sleep(10);
    //mScheduler.StopScheduler();
    // Test 6: Stop without start
    //mScheduler.StopScheduler();
    // Test 7: Other API calls with start Scheduler
    //for(int i=0;i<10;i++)
    //{
    //    AddTask(mScheduler, "LoopTest");
    //    mScheduler.RemoveAllTasks();
    //    mScheduler.SuspendScheduler();
    //    mScheduler.ResumeScheduler();
    //}
    // Test 8 :
    mScheduler.StartScheduler();
    std::thread t1(task1,(void *)&mScheduler);
    std::thread t2(task2,(void *)&mScheduler);
    sleep(10);
    t1.join();
    t2.join();
    mScheduler.StopScheduler();
    return 0;
}
#endif
