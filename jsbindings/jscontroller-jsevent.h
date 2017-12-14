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
* @file JavaScript Event Impl for AAMP_JSController
*/

#ifndef __JSCONTROLLER_JSEVENT_H__
#define __JSCONTROLLER_JSEVENT_H__

#include <JavaScriptCore/JavaScript.h>

enum EventPhase
{
	pNone = 0,
	pCapturingPhase,
	pAtTarget,
	pBubblingPhase
};

class AAMPJSCEvent
{

public:
	AAMPJSCEvent();
	AAMPJSCEvent(const char *type, bool bubble, bool cancelable);
	~AAMPJSCEvent();

	void initEvent(const char *type, bool bubble, bool cancelable);

	const char* getType() 	{ return _typeName; }
	bool getBubbles()       { return _bubbles; }
	bool getCancelable()    { return _cancelable; }
	JSObjectRef getTarget() { return _target; }
	JSObjectRef getCurrentTarget()  { return _currentTarget; }
	EventPhase getEventPhase()  { return _phase; }
	bool getIsDefaultPrevented()  { return _canceled; }
	bool getIsTrusted()           { return _isTrusted; }
	double getTimestamp()       { return _timestamp; }

	void preventDefault()
	{
		if(_cancelable)
			_canceled = true;
	}
	void stopImmediatePropagation()
	{
		_stopImmediatePropagation = true;
	}
	void stopPropagation()
	{
		_stopPropagation = true;
	}

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

JSObjectRef createNewJSEvent(JSGlobalContextRef ctx, const char *type, bool bubbles, bool cancelable);

#endif //__JSCONTROLLER_JSEVENT_H__
