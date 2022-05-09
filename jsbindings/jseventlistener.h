/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file jseventlistener.h
 * @brief Event Listner impl for AAMPMediaPlayer_JS object
 */


#ifndef __AAMP_JSEVENTLISTENER__H__
#define __AAMP_JSEVENTLISTENER__H__


#include "jsbindings.h"


/**
 * @class AAMP_JSEventListener
 * @brief Event listener impl for AAMPMediaPlayer_JS object
 */
class AAMP_JSEventListener : public AAMPEventObjectListener
{
public:
	/**
	 * @brief Adds a JS function as listener for a particular event
	 * @param[in] obj instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	static void AddEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback);
	/**
	 * @brief Removes a JS listener for a particular event
	 * @param[in] obj instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be removed as listener
	 */
	static void RemoveEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback);
	/**
	 * @brief Remove all JS listeners registered
	 * @param[in] obj instance of PrivAAMPStruct_JS
	 */
	static void RemoveAllEventListener(PrivAAMPStruct_JS* obj);
	/**
	 * @brief AAMP_JSEventListener Constructor
	 * @param[in] obj instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback for the event type
	 */
	AAMP_JSEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback);
	/**
	 * @brief AAMP_JSEventListener Destructor
	 */
	~AAMP_JSEventListener();
	/**
         * @brief Copy constructor disabled
         *
         */
	AAMP_JSEventListener(const AAMP_JSEventListener&) = delete;
	/**
 	 * @brief assignment operator disabled
  	 *
 	 */
	AAMP_JSEventListener& operator=(const AAMP_JSEventListener&) = delete;
	/**
	 * @brief Callback invoked for dispatching event
	 * @param[in] e AAMPEventPtr event object
	 */
	void Event(const AAMPEventPtr& e);

	virtual void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
	}

public:
	PrivAAMPStruct_JS* p_obj;   /**< JS execution context to use */
	AAMPEventType p_type;       /**< event type */
	JSObjectRef p_jsCallback;   /**< callback registered for event */
};

#endif /** __AAMP_JSEVENTLISTENER__H__ **/
