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

#include "jscontroller-jsevent.h"
#include "jsutils.h"
#include <stdio.h>

//#define _DEBUG

#ifdef _DEBUG
#define LOG(...)  logprintf(__VA_ARGS__);fflush(stdout);
#else
#define LOG(...)
#endif


AAMPJSCEvent::AAMPJSCEvent()
	: _bubbles(false)
	, _cancelable(false)
	, _canceled(false)
	, _currentTarget(NULL)
	, _defaultPrevented(false)
	, _phase(pAtTarget)
	, _target(NULL)
	, _timestamp(0)
	, _typeName("")
	, _isTrusted(false)
	, _ctx(NULL)
	, _stopImmediatePropagation(false)
	, _stopPropagation(false)
{

}

AAMPJSCEvent::AAMPJSCEvent(const char *type, bool bubble, bool cancelable)
	: _bubbles(bubble)
	, _cancelable(cancelable)
	, _canceled(false)
	, _currentTarget(NULL)
	, _defaultPrevented(false)
	, _phase(pAtTarget)
	, _target(NULL)
	, _timestamp(0)
	, _typeName(type)
	, _isTrusted(false)
	, _ctx(NULL)
	, _stopImmediatePropagation(false)
	, _stopPropagation(false)
{

}

AAMPJSCEvent::~AAMPJSCEvent()
{
	if(_target != NULL)
	{
		JSValueUnprotect(_ctx, _target);
	}

	if(_currentTarget != NULL)
	{
		JSValueUnprotect(_ctx, _currentTarget);
	}
}

void AAMPJSCEvent::initEvent(const char *type, bool bubble, bool cancelable)
{
	_typeName = type;
	_bubbles = bubble;
	_cancelable = cancelable;
}


static void initEvent(JSContextRef context, JSObjectRef thisObj, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	char* evType;
	bool bubbles = false;
	bool cancelable = false;

	if (argumentCount >= 1 && JSValueIsString(context, arguments[0]))
	{
		evType = aamp_JSValueToCString(context, arguments[0], NULL);

		if (argumentCount >= 2 && JSValueIsObject(context, arguments[1]))
		{
			JSObjectRef eventParams = JSValueToObject(context, arguments[1], NULL);

			JSStringRef bubblesProp = JSStringCreateWithUTF8CString("bubbles");
			JSValueRef bubblesValue = JSObjectGetProperty(context, eventParams, bubblesProp, NULL);
			if (JSValueIsBoolean(context, bubblesValue))
			{
				bubbles = JSValueToBoolean(context, bubblesValue);
			}
			JSStringRelease(bubblesProp);

			JSStringRef cancelableProp = JSStringCreateWithUTF8CString("cancelable");
			JSValueRef cancelableValue = JSObjectGetProperty(context, eventParams, cancelableProp, NULL);
			if (JSValueIsBoolean(context, cancelableValue))
			{
				cancelable = JSValueToBoolean(context, cancelableValue);
			}
			JSStringRelease(cancelableProp);
		}

		AAMPJSCEvent* ev = (AAMPJSCEvent*) JSObjectGetPrivate(thisObj);

		if (ev && evType != NULL)
		{
			ev->initEvent(evType, bubbles, cancelable);
		}

		delete[] evType;
	}
}

static JSValueRef AAMPJSCEvent_initEvent(JSContextRef context, JSObjectRef func, JSObjectRef thisObj, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	initEvent(context, thisObj, argumentCount, arguments, exception);

	return JSValueMakeUndefined(context);
}

static JSValueRef AAMPJSCEvent_preventDefault(JSContextRef context, JSObjectRef func, JSObjectRef thisObj, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj != NULL)
	{
		eventObj->preventDefault();
	}

	return JSValueMakeUndefined(context);
}

static JSValueRef AAMPJSCEvent_stopImmediatePropagation(JSContextRef context, JSObjectRef func, JSObjectRef thisObj, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj != NULL)
	{
		eventObj->stopImmediatePropagation();
	}

	return JSValueMakeUndefined(context);
}

static JSValueRef AAMPJSCEvent_stopPropagation(JSContextRef context, JSObjectRef func, JSObjectRef thisObj, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj != NULL)
	{
		eventObj->stopPropagation();
	}

	return JSValueMakeUndefined(context);
}

static JSValueRef AAMPJSCEvent_getproperty_bubbles(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeBoolean(context, eventObj->getBubbles());
}

static JSValueRef AAMPJSCEvent_getproperty_cancelable(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeBoolean(context, eventObj->getCancelable());
}

static JSValueRef AAMPJSCEvent_getproperty_defaultPrevented(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeBoolean(context, eventObj->getIsDefaultPrevented());
}

static JSValueRef AAMPJSCEvent_getproperty_eventPhase(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeNumber(context, eventObj->getEventPhase());
}

static JSValueRef AAMPJSCEvent_getproperty_target(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return eventObj->getTarget();
}

static JSValueRef AAMPJSCEvent_getproperty_currentTarget(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return eventObj->getCurrentTarget();
}

static JSValueRef AAMPJSCEvent_getproperty_timestamp(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeNumber(context, eventObj->getTimestamp());
}

static JSValueRef AAMPJSCEvent_getproperty_type(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return aamp_CStringToJSValue(context, eventObj->getType());
}

static JSValueRef AAMPJSCEvent_getproperty_isTrusted(JSContextRef context, JSObjectRef thisObj, JSStringRef propertyName, JSValueRef* exception)
{
	AAMPJSCEvent *eventObj = (AAMPJSCEvent *) JSObjectGetPrivate(thisObj);

	if (eventObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeBoolean(context, eventObj->getIsTrusted());
}

void AAMPJSCEvent_initialize (JSContextRef ctx, JSObjectRef object)
{
        AAMPJSCEvent* ev = new AAMPJSCEvent();
        JSObjectSetPrivate(object, ev);


}

void AAMPJSCEvent_finalize(JSObjectRef object)
{
	AAMPJSCEvent *ev = (AAMPJSCEvent *) JSObjectGetPrivate(object);

	delete ev;
}

static JSStaticFunction AAMPJSCEvent_static_functions[] =
{
	{ "initEvent", AAMPJSCEvent_initEvent, kJSPropertyAttributeReadOnly },
	{ "preventDefault", AAMPJSCEvent_preventDefault, kJSPropertyAttributeReadOnly },
	{ "stopImmediatePropagation", AAMPJSCEvent_stopImmediatePropagation, kJSPropertyAttributeReadOnly },
	{ "stopPropagation", AAMPJSCEvent_stopPropagation, kJSPropertyAttributeReadOnly },
	{ NULL, NULL, 0 }
};
static JSStaticValue AAMPJSCEvent_static_values[] =
{
	{ "bubbles", AAMPJSCEvent_getproperty_bubbles, NULL, kJSPropertyAttributeReadOnly },
	{ "cancelable", AAMPJSCEvent_getproperty_cancelable, NULL, kJSPropertyAttributeReadOnly },
	{ "defaultPrevented", AAMPJSCEvent_getproperty_defaultPrevented, NULL, kJSPropertyAttributeReadOnly },
	{ "eventPhase", AAMPJSCEvent_getproperty_eventPhase, NULL, kJSPropertyAttributeReadOnly },
	{ "target", AAMPJSCEvent_getproperty_target, NULL, kJSPropertyAttributeReadOnly },
	{ "currentTarget", AAMPJSCEvent_getproperty_currentTarget, NULL, kJSPropertyAttributeReadOnly },
	{ "timestamp", AAMPJSCEvent_getproperty_timestamp, NULL, kJSPropertyAttributeReadOnly },
	{ "type", AAMPJSCEvent_getproperty_type, NULL, kJSPropertyAttributeReadOnly },
	{ "isTrusted", AAMPJSCEvent_getproperty_isTrusted, NULL, kJSPropertyAttributeReadOnly },
	{ NULL, NULL, NULL, 0 }
};

static const JSClassDefinition AAMPJSCEvent_object_def =
{
	0,
	kJSClassAttributeNone,
	"__Event_AAMPJSController",
	NULL,
	AAMPJSCEvent_static_values,
	AAMPJSCEvent_static_functions,
	AAMPJSCEvent_initialize,
	AAMPJSCEvent_finalize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static JSObjectRef AAMPJSCEvent_class_constructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	JSClassRef classDef = JSClassCreate(&AAMPJSCEvent_object_def);
	JSObjectRef eventObj = JSObjectMake(ctx, classDef, NULL);

	initEvent(ctx, eventObj, argumentCount, arguments, exception);
	JSClassRelease(classDef);

	return eventObj;
}

static const JSClassDefinition AAMPJSCEvent_class_def =
{
	0,
	kJSClassAttributeNone,
	"__Event_AAMPJSController_class",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	AAMPJSCEvent_class_constructor,
	NULL,
	NULL
};

JSObjectRef createNewJSEvent(JSGlobalContextRef ctx, const char *type, bool bubbles, bool cancelable)
{
        JSClassRef classDef = JSClassCreate(&AAMPJSCEvent_object_def);
        JSObjectRef eventObj = JSObjectMake(ctx, classDef, NULL);
	JSClassRelease(classDef);

	AAMPJSCEvent *eventPriv = (AAMPJSCEvent *) JSObjectGetPrivate(eventObj);
	eventPriv->initEvent(type, bubbles, cancelable);

	return eventObj;
}
