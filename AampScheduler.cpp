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
 */
void AampScheduler::StartScheduler()
{
	//Turn on thread for processing async operations
	std::lock_guard<std::mutex>lock(mQMutex);
	mSchedulerThread = std::thread(std::bind(&AampScheduler::ExecuteAsyncTask, this));
	mSchedulerRunning = true;
	AAMPLOG_WARN("Started Async Worker Thread");
}

/**
 * @brief To schedule a task to be executed later
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
 */
void AampScheduler::ExecuteAsyncTask()
{
	std::unique_lock<std::mutex>queueLock(mQMutex);
	while (mSchedulerRunning)
	{
		if (mTaskQueue.empty())
		{
			mQCond.wait(queueLock);
		}
		else
		{
			/* DELIA-57121
			Take the execution lock before taking a task from the queue
			otherwise this function could hold a task, out of the queue,
			that cannot be deleted by RemoveAllTasks()!
			Allow the queue to be modified while waiting.*/
			queueLock.unlock();
			std::lock_guard<std::mutex>executionLock(mExMutex);
			queueLock.lock();

			//DELIA-57121 - note: mTaskQueue could have been modified while waiting for execute permission
			if (!mTaskQueue.empty())
			{
				AsyncTaskObj obj = mTaskQueue.front();
				mTaskQueue.pop_front();
				if (obj.mId != AAMP_TASK_ID_INVALID)
				{
					mCurrentTaskId = obj.mId;
					AAMPLOG_INFO("Found entry in function queue!!, task:%s. State:%d",obj.mTaskName.c_str(),mState);
					if( mState != eSTATE_ERROR && mState != eSTATE_RELEASED)
					{
						//Unlock so that new entries can be added to queue while function executes
						queueLock.unlock();

						AAMPLOG_WARN("SchedulerTask Execution:%s",obj.mTaskName.c_str());
						//Execute function
						obj.mTask(obj.mData);
						//May be used in a wait() in future loops, it needs to be locked
						queueLock.lock();
					}
				}
				else
				{
					AAMPLOG_ERR("Scheduler found a task with invalid ID, skip task!");
				}
			}
		}
	}
	AAMPLOG_INFO("Exited Async Worker Thread");
}

/**
 * @brief To remove all scheduled tasks and prevent further tasks from scheduling
 */
void AampScheduler::RemoveAllTasks()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	if(!mLockOut)
	{
		AAMPLOG_WARN("The scheduler is active.  An active task may continue to execute after this function exits.  Call SuspendScheduler() prior to this function to prevent this.");
	}
	if (!mTaskQueue.empty())
	{
		AAMPLOG_WARN("Clearing up %zu entries from mFuncQueue", mTaskQueue.size());
		mTaskQueue.clear();
	}
}

/**
 * @brief To stop scheduler and associated resources
 */
void AampScheduler::StopScheduler()
{
	AAMPLOG_WARN("Stopping Async Worker Thread");
	// Clean up things in queue
	mSchedulerRunning = false;

	//DELIA-57121 allow StopScheduler() to be called without warning from a nonsuspended state and
	//DELIA-57122 not cause an error in ResumeScheduler() below due to trying to unlock an unlocked lock
	if(!mLockOut)
	{
		SuspendScheduler();
	}

	RemoveAllTasks();

	//DELIA-57122 prevent possible deadlock where mSchedulerThread is waiting for mExLock/mExMutex
	ResumeScheduler();
	mQCond.notify_one();
    if (mSchedulerThread.joinable())
        mSchedulerThread.join();
}

/**
 * @brief To acquire execution lock for synchronisation purposes
 */
void AampScheduler::SuspendScheduler()
{
	mExLock.lock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = true;
}

/**
 * @brief To release execution lock
 */
void AampScheduler::ResumeScheduler()
{
	mExLock.unlock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
}

/**
 * @brief To remove a scheduled tasks with ID
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
 */
void AampScheduler::EnableScheduleTask()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
}

/**
 * @brief To player state to Scheduler
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
