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
 * @file jscontroller-jsevent.h
 * @brief JavaScript Event Impl for AAMP_JSController
 */


#ifndef __JSCONTROLLER_JSEVENT_H__
#define __JSCONTROLLER_JSEVENT_H__

#include <JavaScriptCore/JavaScript.h>

/**
 * @enum EventPhase
 * @brief
 */
enum EventPhase
{
	pNone = 0,
	pCapturingPhase,
	pAtTarget,
	pBubblingPhase
};

/**
 * @class AAMPJSCEvent
 * @brief
 */
class AAMPJSCEvent
{

public:

	/**
	 * @brief AAMPJSCEvent Constructor
	 */
	AAMPJSCEvent();

	/**
	 * @brief AAMPJSCEvent Constructor
	 * @param type
	 * @param bubble
	 * @param cancelable
	 */
	AAMPJSCEvent(const char *type, bool bubble, bool cancelable);

	/**
	 * @brief AAMPJSCEvent Destructor
	 */
	~AAMPJSCEvent();


	/**
	 * @brief Initialize JS event
	 * @param type
	 * @param bubble
	 * @param cancelable
	 */
	void initEvent(const char *type, bool bubble, bool cancelable);


	/**
	 * @brief Get event type
	 * @retval event type
	 */
	const char* getType() 	{ return _typeName; }

	/**
	 * @brief
	 * @retval
	 */
	bool getBubbles()       { return _bubbles; }

	/**
	 * @brief
	 * @retval
	 */
	bool getCancelable()    { return _cancelable; }

	/**
	 * @brief
	 * @retval
	 */
	JSObjectRef getTarget() { return _target; }

	/**
	 * @brief
	 * @retval
	 */
	JSObjectRef getCurrentTarget()  { return _currentTarget; }

	/**
	 * @brief
	 * @retval
	 */
	EventPhase getEventPhase()  { return _phase; }

	/**
	 * @brief
	 * @retval
	 */
	bool getIsDefaultPrevented()  { return _canceled; }

	/**
	 * @brief
	 * @retval
	 */
	bool getIsTrusted()           { return _isTrusted; }

	/**
	 * @brief
	 * @retval
	 */
	double getTimestamp()       { return _timestamp; }


	/**
	 * @brief
	 */
	void preventDefault()
	{
		if(_cancelable)
			_canceled = true;
	}

	/**
	 * @brief
	 */
	void stopImmediatePropagation()
	{
		_stopImmediatePropagation = true;
	}

	/**
	 * @brief
	 */
	void stopPropagation()
	{
		_stopPropagation = true;
	}


	/**
	 * @brief
	 * @param context
	 * @param obj
	 */
	void setTarget(JSContextRef context, JSObjectRef obj)
	{
		if (_ctx == NULL)
		{
			_ctx = context;
		}
		if (_target)
		{
			JSValueUnprotect(_ctx, _target);
		}
		_target = obj;
		JSValueProtect(_ctx, _target);
	}


	/**
	 * @brief
	 * @param context
	 * @param obj
	 */
	void setCurrentTarget(JSContextRef context, JSObjectRef obj)
	{
		if (_ctx == NULL)
		{
			_ctx = context;
		}
		if (_currentTarget)
		{
			JSValueUnprotect(_ctx, _currentTarget);
		}
		_currentTarget = obj;
		JSValueProtect(_ctx, _currentTarget);
	}

private:
	bool _bubbles;
	bool _cancelable;
	bool _canceled;
	JSObjectRef _currentTarget;
	bool _defaultPrevented;
	EventPhase _phase;
	JSObjectRef _target;
	double _timestamp;
	const char * _typeName;
	bool _isTrusted;

	bool _stopImmediatePropagation;
	bool _stopPropagation;

	JSContextRef _ctx;
};


/**
 * @brief
 * @param ctx
 * @param type
 * @param bubbles
 * @param cancelable
 * @retval
 */
JSObjectRef createNewJSEvent(JSGlobalContextRef ctx, const char *type, bool bubbles, bool cancelable);

#endif //__JSCONTROLLER_JSEVENT_H__
