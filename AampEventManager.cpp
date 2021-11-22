/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

#include "AampEventManager.h"


//#define EVENT_DEBUGGING 1

/**
 * @brief Get the idle task's source ID
 * @retval source ID
 */
guint AampEventManager::GetSourceID()
{
	guint callbackId = 0;
	GSource *source = g_main_current_source();
	if (source != NULL)
	{
		callbackId = g_source_get_id(source);
	}
	return callbackId;
}


/**
 *	 @brief Default Constructor
 *
 *	 @return void
 */
AampEventManager::AampEventManager(AampLogManager *logObj): mIsFakeTune(false),mLogObj(logObj),
					mAsyncTuneEnabled(false),mEventPriority(G_PRIORITY_DEFAULT_IDLE),mMutexVar(PTHREAD_MUTEX_INITIALIZER),
					mPlayerState(eSTATE_IDLE),mEventWorkerDataQue(),mPendingAsyncEvents()
{
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
	{
		mEventListeners[i]	= NULL;
		mEventStats[i]		=	 0;
	}
}
/**
* @brief Destructor Function
*/
AampEventManager::~AampEventManager()
{

	// Clear all event listners and pending events
	FlushPendingEvents();
	pthread_mutex_lock(&mMutexVar);
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
	{
		while (mEventListeners[i] != NULL)
		{
			ListenerData* pListener = mEventListeners[i];
			mEventListeners[i] = pListener->pNext;
			delete pListener;
		}
	}
	pthread_mutex_unlock(&mMutexVar);
	pthread_mutex_destroy(&mMutexVar);
}

/**
 * @brief FlushPendingEvents - Flush Remaining events in the queue
 * @return void
 */
void AampEventManager::FlushPendingEvents()
{
	pthread_mutex_lock(&mMutexVar);
	while(!mEventWorkerDataQue.empty())
	{
		// Remove each AampEventPtr from the queue , not deleting the Shard_ptr
		mEventWorkerDataQue.pop();
	}

	if (mPendingAsyncEvents.size() > 0)
	{
		AAMPLOG_WARN("mPendingAsyncEvents.size - %d", mPendingAsyncEvents.size());
		for (AsyncEventListIter it = mPendingAsyncEvents.begin(); it != mPendingAsyncEvents.end(); it++)
		{
			if ((it->first != 0) && (it->second))
			{
				AAMPLOG_WARN("remove id - %d", (int) it->first);
				g_source_remove(it->first);

			}
		}
		mPendingAsyncEvents.clear();
	}

	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
		mEventStats[i] = 0;
	pthread_mutex_unlock(&mMutexVar);

#ifdef EVENT_DEBUGGING
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
	{
		logprintf("%s EventType[%d]->[%d]",__FUNCTION__,i,mEventStats[i]);
	}
#endif

}

/**
 * @brief AddListenerForAllEvents - Register one listener for all events
 * @param EventListener - listerner for events
 * @return void
 */
void AampEventManager::AddListenerForAllEvents(EventListener* eventListener)
{
	if(eventListener != NULL)
	{
		AddEventListener(AAMP_EVENT_ALL_EVENTS,eventListener);
	}
	else
	{
		AAMPLOG_ERR("Null eventlistner. Failed to register");
	}
}

/**
 * @brief RemoveListenerForAllEvents - Remove listener for all events
 * @param EventListener - listerner for events
 * @return void
 */
void AampEventManager::RemoveListenerForAllEvents(EventListener* eventListener)
{
	if(eventListener != NULL)
	{
		RemoveEventListener(AAMP_EVENT_ALL_EVENTS,eventListener);
	}
	else
	{
		AAMPLOG_ERR("Null eventlistner. Failed to deregister");
	}
}

/**
 * @brief Add listener to aamp events
 * @param eventType type of event to subscribe
 * @param eventListener listener
 */
void AampEventManager::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	if ((eventListener != NULL) && (eventType >= AAMP_EVENT_ALL_EVENTS) && (eventType < AAMP_MAX_NUM_EVENTS))
	{
		ListenerData* pListener = new ListenerData;
		if (pListener)
		{
			AAMPLOG_INFO("EventType:%d, Listener %p new %p", eventType, eventListener, pListener);
			pthread_mutex_lock(&mMutexVar);
			pListener->eventListener = eventListener;
			pListener->pNext = mEventListeners[eventType];
			mEventListeners[eventType] = pListener;
			pthread_mutex_unlock(&mMutexVar);
		}
	}
	else
	{
		AAMPLOG_ERR("EventType %d registered out of range",eventType);
	}
}


/**
 * @brief Remove listener to aamp events
 * @param eventType type of event to unsubscribe
 * @param eventListener listener
 */
void AampEventManager::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	// listner instance is cleared here , but created outside
	if ((eventListener != NULL) && (eventType >= AAMP_EVENT_ALL_EVENTS) && (eventType < AAMP_MAX_NUM_EVENTS))
	{
		pthread_mutex_lock(&mMutexVar);
		ListenerData** ppLast = &mEventListeners[eventType];
		while (*ppLast != NULL)
		{
			ListenerData* pListener = *ppLast;
			if (pListener->eventListener == eventListener)
			{
				*ppLast = pListener->pNext;
				pthread_mutex_unlock(&mMutexVar);
				AAMPLOG_INFO("Eventtype:%d %p delete %p", eventType, eventListener, pListener);
				delete pListener;
				return;
			}
			ppLast = &(pListener->pNext);
		}
		pthread_mutex_unlock(&mMutexVar);
	}
}

/**
 * @brief IsEventListenerAvailable - Check if any listners present for this event
 * @param AAMPEventType - Event Type
 * @return True if listner present
 */
bool AampEventManager::IsSpecificEventListenerAvailable(AAMPEventType eventType)
{	
	bool retVal=false;
	pthread_mutex_lock(&mMutexVar);
	if(eventType > AAMP_EVENT_ALL_EVENTS &&  eventType < AAMP_MAX_NUM_EVENTS && mEventListeners[eventType])
	{
		retVal = true;
	}
	pthread_mutex_unlock(&mMutexVar);
	return retVal;
}

bool AampEventManager::IsEventListenerAvailable(AAMPEventType eventType)
{
	bool retVal=false;
	pthread_mutex_lock(&mMutexVar);
	if(eventType >= AAMP_EVENT_TUNED &&  eventType < AAMP_MAX_NUM_EVENTS && (mEventListeners[AAMP_EVENT_ALL_EVENTS] || mEventListeners[eventType]))
	{
		retVal = true;
	}
	pthread_mutex_unlock(&mMutexVar);
	return retVal;
}
/**
 * @brief SetFakeTuneFlag - Some events are restricted for FakeTune
 * @param boolean - True for FakeTune
 * @return void
 */
void AampEventManager::SetFakeTuneFlag(bool isFakeTuneSetting)
{
	pthread_mutex_lock(&mMutexVar);
	mIsFakeTune = isFakeTuneSetting;
	pthread_mutex_unlock(&mMutexVar);
}

void AampEventManager::SetAsyncTuneState(bool isAsyncTuneSetting)
{
	pthread_mutex_lock(&mMutexVar);
	mAsyncTuneEnabled = isAsyncTuneSetting;
	if(mAsyncTuneEnabled)
	{
		mEventPriority = AAMP_MAX_EVENT_PRIORITY;
	}
	else
	{
		mEventPriority = G_PRIORITY_DEFAULT_IDLE;
	}
	pthread_mutex_unlock(&mMutexVar);
}

/**
 * @brief SetPlayerState - Flag to update player state
 * @param PrivAAMPState - Player state
 * @return void
 */
void AampEventManager::SetPlayerState(PrivAAMPState state)
{
	pthread_mutex_lock(&mMutexVar);
	mPlayerState = state;
	pthread_mutex_unlock(&mMutexVar);
}

/**
 * @brief SendEvent - Generic function to send events
 * @param AAMPEventPtr - Event data
 * @return void
 */
void AampEventManager::SendEvent(const AAMPEventPtr &eventData, AAMPEventMode eventMode)
{
	// If some event wants to override  to send as Sync ,then override flag to be set
	// This will go as Sync only if SourceID of thread != 0 , else there is assert catch in SendEventSync
	AAMPEventType eventType = eventData->getType();
	if ((eventType < AAMP_EVENT_ALL_EVENTS) || (eventType >= AAMP_MAX_NUM_EVENTS))  //CID:81883 - Resolve OVER_RUN
                return;

	if(eventMode < AAMP_EVENT_DEFAULT_MODE || eventMode > 	AAMP_EVENT_ASYNC_MODE)
		eventMode = AAMP_EVENT_DEFAULT_MODE;

	if((mPlayerState != eSTATE_RELEASED) && (mEventListeners[AAMP_EVENT_ALL_EVENTS] || mEventListeners[eventType]))
	{
		guint sId = GetSourceID();
		// if caller is asking for Sync Event , ensure it has proper source Id, else it has to go async event
		if(eventMode==AAMP_EVENT_SYNC_MODE &&  sId != 0)
		{
			SendEventSync(eventData);
		}
		else if(eventMode==AAMP_EVENT_ASYNC_MODE)
		{
			SendEventAsync(eventData);
		}
		else
		{
			//For other events if asyncTune eneabled or calle from non-UI thread , then send the event as Async
			if (mAsyncTuneEnabled || sId == 0)
			{
				SendEventAsync(eventData);
			}
			else
			{
				SendEventSync(eventData);
			}
		}
	}
}


// Worker thread for handling Async Events
void AampEventManager::AsyncEvent()
{
	pthread_mutex_lock(&mMutexVar);
	AAMPEventPtr eventData=NULL;
	// pop out the event to sent in async mode
	if(mEventWorkerDataQue.size())
	{
		eventData = (AAMPEventPtr)mEventWorkerDataQue.front();
		mEventWorkerDataQue.pop();
	}
	pthread_mutex_unlock(&mMutexVar);
	// Push the new event in sync mode from the idle task
	if(eventData && (IsEventListenerAvailable(eventData->getType())) && (mPlayerState != eSTATE_RELEASED)) {
		SendEventSync(eventData);
	}
}

/**
 * @brief Send event asynchronously to listeners
 * @param e event
 */
void AampEventManager::SendEventAsync(const AAMPEventPtr &eventData)
{
	AAMPEventType eventType = eventData->getType();
	pthread_mutex_lock(&mMutexVar);
	// Check if already player in release state , then no need to send any events
	if(mPlayerState != eSTATE_RELEASED)
	{
		AAMPLOG_INFO("Sending event %d to AsyncQ", eventType);
		mEventWorkerDataQue.push(eventData);
		pthread_mutex_unlock(&mMutexVar);
		// Every event need a idle task to execute it
		guint callbackID = g_idle_add_full(mEventPriority, EventManagerThreadFunction, this, NULL);
		if(callbackID != 0)
		{
			SetCallbackAsPending(callbackID);
		}
	}
	pthread_mutex_unlock(&mMutexVar);
	return;
}


/**
 * @brief Send event synchronously to listeners
 * @param e event
 */
void AampEventManager::SendEventSync(const AAMPEventPtr &eventData)
{
	AAMPEventType eventType = eventData->getType();
	pthread_mutex_lock(&mMutexVar);
#ifdef EVENT_DEBUGGING
	long long startTime = NOW_STEADY_TS_MS;
#endif
	// Check if already player in release state , then no need to send any events
	// Its checked again here ,as async events can come to sync mode after playback is stopped 
	if(mPlayerState == eSTATE_RELEASED)
	{
		pthread_mutex_unlock(&mMutexVar);
		return;
	}
	
	if(mIsFakeTune && !(AAMP_EVENT_STATE_CHANGED == eventType && eSTATE_COMPLETE == std::dynamic_pointer_cast<StateChangedEvent>(eventData)->getState()) && !(AAMP_EVENT_EOS == eventType))
	{
		AAMPLOG_TRACE("Events are disabled for fake tune");
		pthread_mutex_unlock(&mMutexVar);
		return;
	}
	mEventStats[eventType]++;
	if (eventType != AAMP_EVENT_PROGRESS)
	{
		if (eventType != AAMP_EVENT_STATE_CHANGED)
		{
			AAMPLOG_INFO("(type=%d)", eventType);
		}
		else
		{
			AAMPLOG_WARN("(type=%d)(state=%d)", eventType, std::dynamic_pointer_cast<StateChangedEvent>(eventData)->getState());
		}
	}

	// Build list of registered event listeners.
	ListenerData* pList = NULL;
	ListenerData* pListener = mEventListeners[eventType];
	while (pListener != NULL)
	{
		ListenerData* pNew = new ListenerData;
		pNew->eventListener = pListener->eventListener;
		pNew->pNext = pList;
		pList = pNew;
		pListener = pListener->pNext;
	}
	pListener = mEventListeners[AAMP_EVENT_ALL_EVENTS];  // listeners registered for "all" event types
	while (pListener != NULL)
	{
		ListenerData* pNew = new ListenerData;
		pNew->eventListener = pListener->eventListener;
		pNew->pNext = pList;
		pList = pNew;
		pListener = pListener->pNext;
	}
	pthread_mutex_unlock(&mMutexVar);

	// After releasing the lock, dispatch each of the registered listeners.
	// This allows event handlers to add/remove listeners for future events.
	while (pList != NULL)
	{
		ListenerData* pCurrent = pList;
		if (pCurrent->eventListener != NULL)
		{
			//logprintf("[AAMP_JS] %s(type=%d) listener=%p", __FUNCTION__, eventType, pCurrent->eventListener);
			pCurrent->eventListener->SendEvent(eventData);
		}
		pList = pCurrent->pNext;
		SAFE_DELETE(pCurrent);
	}
#ifdef EVENT_DEBUGGING
	logprintf("%s TimeTaken for Event %d SyncEvent [%d]",__FUNCTION__,eventType, (NOW_STEADY_TS_MS - startTime));
#endif

}


/**
 * @brief Set an idle callback to dispatched state
 * @param id Idle task Id
 */
void AampEventManager::SetCallbackAsDispatched(guint id)
{
	pthread_mutex_lock(&mMutexVar);
	AsyncEventListIter  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		AAMPLOG_INFO("id:%d in mPendingAsyncEvents, erasing it. State:%d", id,itr->second);
		assert (itr->second);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		AAMPLOG_WARN("id:%d not in mPendingAsyncEvents, insert and mark as not pending", id);
		mPendingAsyncEvents[id] = false;
	}
	pthread_mutex_unlock(&mMutexVar);
}

/**
 * @brief Set an idle callback to pending state
 * @param id Idle task Id
 */
void AampEventManager::SetCallbackAsPending(guint id)
{
	pthread_mutex_lock(&mMutexVar);
	AsyncEventListIter  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		AAMPLOG_WARN("id:%d already in mPendingAsyncEvents and completed, erase it State:%d",id,itr->second);
		assert (!itr->second);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		mPendingAsyncEvents[id] = true;
		AAMPLOG_INFO("id:%d in mPendingAsyncEvents, added to list", id);
	}
	pthread_mutex_unlock(&mMutexVar);
}
