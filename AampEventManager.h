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
 * @struct ListenerData
 * @brief  Structure of the event listener list
 */
struct ListenerData {
	EventListener* eventListener;   /**< Event listener */
	ListenerData* pNext;            /**< Next listener */
};

/**
 * @class AampEventManager
 * @brief Class to Handle Aamp Events
 */ 

class AampEventManager
{

private:
	bool mIsFakeTune;			 		  /**< Flag indicating if fake tune enabled or not  */
	bool mAsyncTuneEnabled;			   		  /**< Flag indicating if Async tune enabled or not  */
	PrivAAMPState mPlayerState;	    		  	  /**< Player state flag , updated only at start and Release */
	pthread_mutex_t mMutexVar ;        			  /**< Mutex variable to handle pending and dispatch operation */
	int mEventPriority;		    			  /**< Async Event task Priority  */
	// Separate registration for each event
	ListenerData* mEventListeners[AAMP_MAX_NUM_EVENTS];	  /**< Event listener registration */
	int mEventStats[AAMP_MAX_NUM_EVENTS];			  /**< Event stats */
	typedef std::queue<AAMPEventPtr> EventWorkerDataQ;	  /**< Event Queue for Async processing  */
	EventWorkerDataQ mEventWorkerDataQue;
	typedef std::map<guint, bool> AsyncEventList;		  /**< Collection of Async tasks pending */
	typedef std::map<guint, bool>::iterator AsyncEventListIter;
	AsyncEventList	mPendingAsyncEvents;
	AampLogManager *mLogObj;
private:
	/**
	 * @fn AsyncEvent
	 * @return void
	 */
	void AsyncEvent();
	/**
	 * @fn GetSourceID
	 */
	guint GetSourceID();
	/**
	 * @fn SetCallbackAsDispatched
	 * @param id - CallbackId for the IdleEvent
	 * @return void
	 */
	void SetCallbackAsDispatched(guint id);
	/**
	 * @fn SetCallbackAsPending
	 * @param id - CallbackId for the IdleEvent
	 * @return void
	 */
	void SetCallbackAsPending(guint id);
	/**
	 * @brief Thread entry function for Async Event Processing
	 */
	static gboolean EventManagerThreadFunction(gpointer This)
	{
		guint callbackId =	g_source_get_id(g_main_current_source());
		AampEventManager *evtMgr = (AampEventManager *)This;
		evtMgr->SetCallbackAsDispatched(callbackId);
		evtMgr->AsyncEvent();
		return G_SOURCE_REMOVE ;
	}
	/**
	 * @fn SendEventAsync
	 * @param eventData - Event data
	 * @return void
	 */
	void SendEventAsync(const AAMPEventPtr &eventData);
	/**
	 * @fn SendEventSync
	 * @param eventData - Event data
	 * @return void
	 */
	void SendEventSync(const AAMPEventPtr &eventData);

public:

	/**
	 * @fn AampEventManager
	 * @return void
	 */
	AampEventManager(AampLogManager *logObj);
	/**
	 * @fn ~AampEventManager
	 */
	~AampEventManager();
	/**
	 * @fn SetFakeTuneFlag 
	 * @param isFakeTuneSetting - True for FakeTune
	 * @return void
	 */
	void SetFakeTuneFlag(bool isFakeTuneSetting);
	/**
	 * @fn SetAsyncTuneState
	 * @param isAsyncTuneSetting - True for Async tune
	 * @return void
	 */
	void SetAsyncTuneState(bool isAsyncTuneSetting);
	/**
	 * @fn SetPlayerState
	 * @param state - Aamp Player state
	 * @return void
	 */
	void SetPlayerState(PrivAAMPState state);
	/**
	 * @fn SendEvent
	 * @param eventData - Event data
	 * @param eventMode - Aamp Event mode
	 * @return void
	 */
	void SendEvent(const AAMPEventPtr &eventData, AAMPEventMode eventMode=AAMP_EVENT_DEFAULT_MODE);
	/**
	 * @fn AddListenerForAllEvents
	 * @param eventListener - listerner for events
	 * @return void
	 */
	void AddListenerForAllEvents(EventListener* eventListener);
	/**
	 * @fn RemoveListenerForAllEvents
	 * @param eventListener - listerner for events
	 * @return void
	 */	
	void RemoveListenerForAllEvents(EventListener* eventListener);
	/**
	 * @fn AddEventListener
	 * @param eventType - Aamp Event type
	 * @param eventListener - listerner for events
	 * @return void
	 */
	void AddEventListener(AAMPEventType eventType, EventListener* eventListener);
	/**
	 * @fn RemoveEventListener
	 * @param eventType - Aamp Event type
	 * @param eventListener - listerner for events
	 * @return void
	 */
	void RemoveEventListener(AAMPEventType eventType, EventListener* eventListener);
	/**
	 * @fn IsEventListenerAvailable - Check if any listners present for this event
	 * @param eventType - Aamp Event Type
	 * @return True if listner present
	 */
	bool IsEventListenerAvailable(AAMPEventType eventType);
	/**
	 * @fn IsSpecificEventListenerAvailable
	 * @param eventType - Event Type
	 * @return True if listner present
	 */
	bool IsSpecificEventListenerAvailable(AAMPEventType eventType);
	/**
	 * @fn FlushPendingEvents
	 * @return void
	 */
	void FlushPendingEvents();
	/**
         * @brief Copy constructor disabled
         *
         */
	AampEventManager(const AampEventManager&) = delete;
	/**
         * @brief assignment operator disabled
         *
         */
	AampEventManager& operator=(const AampEventManager&) = delete;
};




#endif
