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

/**
 * @file AampEventManager.h
 * @brief Event Handler for AAMP Player
 */

#ifndef AAMP_EVENT_MANAGER_H
#define AAMP_EVENT_MANAGER_H

#include "AampDefine.h"
#include "AampConfig.h"
#include "AampEvent.h"
#include "AampEventListener.h"
#include "AampLogManager.h"
#include "AampUtils.h"
#include <pthread.h>
#include <signal.h>
#include <mutex>
#include <queue>
#include <glib.h>


/**
 * @brief  Structure of the event listener list
 */
struct ListenerData {
	EventListener* eventListener;   /**< Event listener */
	ListenerData* pNext;                /**< Next listener */
};


class AampEventManager
{

private:
	bool mIsFakeTune;		/**< Flag indicating if fake tune enabled or not  */
	bool mAsyncTuneEnabled;		/**< Flag indicating if Async tune enabled or not  */
	PrivAAMPState mPlayerState;	/**< Player state flag , updated only at start and Release */
	pthread_mutex_t mMutexVar ;
	int mEventPriority;		/**< Async Event task Priority  */
	// Separate registration for each event
	ListenerData* mEventListeners[AAMP_MAX_NUM_EVENTS];		/**< Event listener registration */
	int mEventStats[AAMP_MAX_NUM_EVENTS];				/**< Event stats */
	typedef std::queue<AAMPEventPtr> EventWorkerDataQ;		/**< Event Queue for Async processing  */
	EventWorkerDataQ mEventWorkerDataQue;
	typedef std::map<guint, bool> AsyncEventList;			/**< Collection of Async tasks pending */
	typedef std::map<guint, bool>::iterator AsyncEventListIter;
	AsyncEventList	mPendingAsyncEvents;
	AampLogManager *mLogObj;
private:
	/**
	 * @brief AsyncEvent - Task function for IdleEvent
	 * @return void
	 */
	void AsyncEvent();
	/**
	 * @brief GetSourceID - GetSourceId of the function
	 * @return Source Id
	 */
	guint GetSourceID();
	/**
	 * @brief SetCallbackAsDispatched - Set callbackId as dispatched/done
	 * @param guint - CallbackId for the IdleEvent
	 * @return void
	 */
	void SetCallbackAsDispatched(guint id);
	/**
	 * @brief SetCallbackAsPending - Set callbackId as Pending/to be done
	 * @param guint - CallbackId for the IdleEvent
	 * @return void
	 */
	void SetCallbackAsPending(guint id);
	// Thread entry function for Async Event Processing
	static gboolean EventManagerThreadFunction(gpointer This)
	{
		guint callbackId =	g_source_get_id(g_main_current_source());
		AampEventManager *evtMgr = (AampEventManager *)This;
		evtMgr->AsyncEvent();
		evtMgr->SetCallbackAsDispatched(callbackId);
		return G_SOURCE_REMOVE ;
	}
	/**
	 * @brief SendEventAsync - Function to send events Async
	 * @param AAMPEventPtr - Event data
	 * @return void
	 */
	void SendEventAsync(const AAMPEventPtr &eventData);
	/**
	 * @brief SendEventSync - Function to send events sync
	 * @param AAMPEventPtr - Event data
	 * @return void
	 */
	void SendEventSync(const AAMPEventPtr &eventData);

public:

	/**
	 *	 @brief Default Constructor
	 *
	 *	 @return void
	 */
	AampEventManager(AampLogManager *logObj);

	/**
	* @brief Destructor Function
	*/
	~AampEventManager();


	/**
	 * @brief SetFakeTuneFlag - Some events are restricted for FakeTune
	 * @param boolean - True for FakeTune
	 * @return void
	 */
	void SetFakeTuneFlag(bool isFakeTuneSetting);
	/**
	 * @brief SetAsyncTuneState - Flag for Async Tune
	 * @param boolean - True for Async tune
	 * @return void
	 */
	void SetAsyncTuneState(bool isAsyncTuneSetting);
	/**
	 * @brief SetPlayerState - Flag to update player state
	 * @param PrivAAMPState - Player state
	 * @return void
	 */
	void SetPlayerState(PrivAAMPState state);
	/**
	 * @brief SendEvent - Generic function to send events
	 * @param AAMPEventPtr - Event data
	 * @return void
	 */
	void SendEvent(const AAMPEventPtr &eventData, AAMPEventMode eventMode=AAMP_EVENT_DEFAULT_MODE);
	/**
	 * @brief AddListenerForAllEvents - Register one listener for all events
	 * @param EventListener - listerner for events
	 * @return void
	 */
	void AddListenerForAllEvents(EventListener* eventListener);
	/**
	 * @brief RemoveListenerForAllEvents - Remove listener for all events
	 * @param EventListener - listerner for events
	 * @return void
	 */	
	void RemoveListenerForAllEvents(EventListener* eventListener);
	/**
	 * @brief AddEventListener - Register  listener for one eventtype
	 * @param EventListener - listerner for events
	 * @return void
	 */
	void AddEventListener(AAMPEventType eventType, EventListener* eventListener);
	/**
	 * @brief RemoveEventListener - Remove one listener registration for one event
	 * @param EventListener - listerner for events
	 * @return void
	 */
	void RemoveEventListener(AAMPEventType eventType, EventListener* eventListener);
	/**
	 * @brief IsEventListenerAvailable - Check if any listners present for this event
	 * @param AAMPEventType - Event Type
	 * @return True if listner present
	 */
	bool IsEventListenerAvailable(AAMPEventType eventType);
	/**
	 * @brief IsSpecificEventListenerAvailable - Check if this particular listener present for this event
	 * @param AAMPEventType - Event Type
	 * @return True if listner present
	 */
	bool IsSpecificEventListenerAvailable(AAMPEventType eventType);

	/**
	 * @brief FlushPendingEvents - Clear all pending events from EventManager
	 * @return void
	 */
	void FlushPendingEvents();

	// Copy constructor and Copy assignment disabled
	AampEventManager(const AampEventManager&) = delete;
	AampEventManager& operator=(const AampEventManager&) = delete;
};




#endif
