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
 * @file AampEventListener.h
 * @brief Classes for AAMP Event listener.
 */

#ifndef __AAMP_EVENT_LISTENER_H__
#define __AAMP_EVENT_LISTENER_H__

#include "AampEvent.h"

/**
 * @class EventListener
 * @brief Class for sed event to Listener
 */

class EventListener
{
public:
	/**
	 * @brief API to send event to event listener
	 * Additional processing can be done here if required
	 *
	 * @param[in] event - AAMPEventPtr object
	 */
	virtual void SendEvent(const AAMPEventPtr &event) = 0;

	/**
	 * @brief EventListener destructor.
	 */
	virtual ~EventListener() { };
};

/**
 * @class AAMPEventListener
 * @brief Class for AAMP event listening [LEGACY]
 * TODO: Deprecate later, AAMPEventObjectListener will be used in future
 */
class AAMPEventListener : public EventListener
{
public:
	/**
	 * @fn SendEvent
	 *
	 * @param[in] event - AAMPEventPtr object
	 */
	void SendEvent(const AAMPEventPtr &event) override;

	/**
	 * @brief API in which event payload is received.
	 * Will be overridden by application and its event processing is done here
	 *
	 * @param[in] event - AAMPEvent data
	 */
	virtual void Event(const AAMPEvent &event) = 0;

	/**
	 * @brief AAMPEventListener destructor.
	 */
	virtual ~AAMPEventListener() { };
};

/**
 * @class AAMPEventObjectListener
 * @brief Class for AAMP event listening
 * Uses shared_ptr for event objects for better memory management
 * New AAMP integration layers should use this event listener for event processing
 */
class AAMPEventObjectListener : public EventListener
{
public:
	/**
	 * @fn SendEvent
	 *
	 * @param[in] event - AAMPEventPtr object
	 */
	void SendEvent(const AAMPEventPtr &event) override;

	/**
	 * @brief API in which event payload is received.
	 * Will be overridden by application and its event processing is done here
	 *
	 * @param[in] event - AAMPEventPtr data
	 */
	virtual void Event(const AAMPEventPtr &event) = 0;

	/**
	 * @brief AAMPEventListener destructor.
	 */
	virtual ~AAMPEventObjectListener() { };
};

#endif /* __AAMP_EVENT_LISTENER_H__ */
