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
* @file JavaScript bindings for AAMP_JSController
*/

#include <JavaScriptCore/JavaScript.h>

#include "priv_aamp.h"
#include "jscontroller-jsevent.h"
#include "jsutils.h"

#include <stdio.h>
#include <string.h>

extern "C"
{
	void aamp_LoadJSController(JSGlobalContextRef context);
	void aamp_UnloadJSController(JSGlobalContextRef context);

	void setAAMPPlayerInstance(PlayerInstanceAAMP *, int);
	void unsetAAMPPlayerInstance(PlayerInstanceAAMP *);
}

class AAMPJSCEventListener;

struct AAMP_JSController
{
	JSGlobalContextRef _ctx;
	PlayerInstanceAAMP *_player;

	int _aampSessionID;
	bool _closedCaptionEnabled;

	AAMPJSCEventListener *_eventListeners;
};

AAMP_JSController* _globalController = NULL;

#ifdef AAMP_CC_ENABLED
//TODO: Fix Race between gstaamp and AAMP_JSController for reading CC config.
//AAMP_JSController is invoked earlier than gstaamp. Whereas gstaamp loads aamp.cfg
static bool aamp_getClosedCaptionConfig()
{
	return aamp_IsCCEnabled();
}

static void aamp_setClosedCaptionStatus(AAMP_JSController *obj, bool status)
{
	if(!aamp_IsCCEnabled())
	{
		return;
	}

	LOG("Set CC status : %d \n", status);
	if(status == true)
	{
		obj->_closedCaptionEnabled = true;
		aamp_CCShow();
	}
	else
	{
		obj->_closedCaptionEnabled = false;
		aamp_CCHide();
	}

}
#endif

namespace AAMPJSController
{

static void dispatchEventToJS(JSContextRef context, JSObjectRef callback, JSObjectRef event)
{
	JSValueRef args[1] = { event };
	if (context != NULL && callback != NULL)
	{
		JSObjectCallAsFunction(context, callback, NULL, 1, args, NULL);
	}
}

}//namespace

class AAMPJSCEventListener : public AAMPEventListener
{
public:
	static void AddEventListener(AAMP_JSController *jsObj, AAMPEventType type, JSObjectRef jsCallback);
	static void RemoveEventListener(AAMP_JSController *jsObj, AAMPEventType type, JSObjectRef jsCallback);
	static void RemoveAllEventListener(AAMP_JSController *jsObj);

	AAMPJSCEventListener(AAMP_JSController* obj, AAMPEventType type, JSObjectRef jsCallback)
		: p_aamp(obj)
		, p_type(type)
		, p_jsCallback(jsCallback)
		, p_next(NULL)
	{
		JSValueProtect(p_aamp->_ctx, p_jsCallback);
	}

	~AAMPJSCEventListener()
	{
		JSValueUnprotect(p_aamp->_ctx, p_jsCallback);
	}

	void Event(const AAMPEvent& e)
	{
		LOG("[AAMP_JSController] %s() type=%d, jsCallback=%p", __FUNCTION__, e.type, p_jsCallback);

		if (p_aamp->_ctx == NULL || e.type >= AAMP_MAX_NUM_EVENTS)
		{
			return;
		}

		JSObjectRef event = createNewJSEvent(p_aamp->_ctx, aamp_getNameFromEventType(e.type), false, false);
		JSValueProtect(p_aamp->_ctx, event);
		AAMPJSController::dispatchEventToJS(p_aamp->_ctx, p_jsCallback, event);
		JSValueUnprotect(p_aamp->_ctx, event);
	}

public:
	AAMP_JSController *p_aamp;
	AAMPEventType p_type;
	JSObjectRef p_jsCallback;
	AAMPJSCEventListener *p_next;
};

void AAMPJSCEventListener::AddEventListener(AAMP_JSController *jsObj, AAMPEventType type, JSObjectRef jsCallback)
{
	AAMPJSCEventListener *newListener = new AAMPJSCEventListener(jsObj, type, jsCallback);
	LOG("[AAMP_JSController] AAMPJSCEventListener::AddEventListener (%d, %p, %p)", type, jsCallback, newListener);
	newListener->p_next = jsObj->_eventListeners;
	jsObj->_eventListeners = newListener;
	if (jsObj->_player != NULL)
	{
		jsObj->_player->AddEventListener(type, newListener);
	}
}

void AAMPJSCEventListener::RemoveEventListener(AAMP_JSController *jsObj, AAMPEventType type, JSObjectRef jsCallback)
{
	LOG("[AAMP_JSController] AAMPJSCEventListener::RemoveEventListener (%d, %p)", type, jsCallback);
	AAMPJSCEventListener *prev = jsObj->_eventListeners;
	AAMPJSCEventListener *curr = prev;

	while (curr != NULL)
	{
		if ((curr->p_type == type) && (curr->p_jsCallback == jsCallback))
		{
			if (curr == jsObj->_eventListeners)
			{
				jsObj->_eventListeners = curr->p_next;
			}
			else
			{
				prev->p_next = curr->p_next;
			}

			if (jsObj->_player != NULL)
			{
				jsObj->_player->RemoveEventListener(type, curr);
			}
			delete curr;
			break;
		}
		prev = curr;
		curr = curr->p_next;
	}
}

void AAMPJSCEventListener::RemoveAllEventListener(AAMP_JSController *jsObj)
{
	AAMPJSCEventListener *iter = jsObj->_eventListeners;
	while (iter != NULL)
	{
		AAMPJSCEventListener *tmp = iter;
		iter = iter->p_next;
		if(jsObj->_player != NULL)
		{
			jsObj->_player->RemoveEventListener(iter->p_type, iter);
		}
		delete tmp;
	}

	jsObj->_eventListeners = NULL;
}

void setAAMPPlayerInstance(PlayerInstanceAAMP *aamp, int sessionID)
{
	LOG("[AAMP_JSController] setAAMPPlayerInstance (%p, id=%d)", aamp, sessionID);
	if (_globalController == NULL)
	{
		return;
	}

	_globalController->_player = aamp;
	_globalController->_aampSessionID = sessionID;

#ifdef AAMP_CC_ENABLED
	// Load CC related configuration values to JSObject
	if (aamp_getClosedCaptionConfig() == true)
	{
		_globalController->_closedCaptionEnabled = true;
	}
        else
	{
		_globalController->_closedCaptionEnabled = false;
	}
#endif
	if (_globalController->_eventListeners != NULL)
	{
		AAMPJSCEventListener *iter = _globalController->_eventListeners;
		while (iter != NULL)
		{
			LOG("[AAMP_JSController] Adding _eventListener(%p) for type(%d) to player ", iter, iter->p_type);
			_globalController->_player->AddEventListener(iter->p_type, iter);
			iter = iter->p_next;
		}
	}
}

void unsetAAMPPlayerInstance(PlayerInstanceAAMP *aamp)
{
        if (_globalController == NULL || _globalController->_player != aamp)
        {
                return;
        }

        if (_globalController->_eventListeners != NULL)
        {
                AAMPJSCEventListener *iter = _globalController->_eventListeners;
                while (iter != NULL)
                {
                        _globalController->_player->RemoveEventListener(iter->p_type, iter);
                        iter = iter->p_next;
                }
        }
        _globalController->_player = NULL;
}


static JSValueRef AAMPJSC_getProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JSController] %s()\n", __FUNCTION__);
	AAMP_JSController* obj = (AAMP_JSController*) JSObjectGetPrivate(thisObject);

	if (obj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.closedCaptionEnabled on instances of AAMP_JSController");
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeBoolean(context, obj->_closedCaptionEnabled);
}

static bool AAMPJSC_setProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JSController] %s()\n", __FUNCTION__);
	AAMP_JSController* obj = (AAMP_JSController*) JSObjectGetPrivate(thisObject);

	if (obj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.closedCaptionEnabled on instances of AAMP_JSController");
		return false;
	}
	else if (!JSValueIsBoolean(context, value))
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.closedCaptionEnabled' - value passed is not boolean");
		return false;

	}

#ifdef AAMP_CC_ENABLED
	bool status = JSValueToBoolean (context, value);
	aamp_setClosedCaptionStatus(obj, status);
#else
	*exception = aamp_GetException(context, AAMPJS_GENERIC_ERROR, "CC is not configured in aamp!");
	return false;
#endif
	return true;
}

static JSValueRef AAMPJSC_getProperty_aampSessionID(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JSController] %s() \n", __FUNCTION__);

	AAMP_JSController *aampObj = (AAMP_JSController *) JSObjectGetPrivate(thisObject);

	if (aampObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	if (aampObj->_aampSessionID == 0)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeNumber(context, aampObj->_aampSessionID);
}

static const JSStaticValue AAMP_JSController_static_values[] =
{
	{"closedCaptionEnabled", AAMPJSC_getProperty_closedCaptionEnabled, AAMPJSC_setProperty_closedCaptionEnabled, kJSPropertyAttributeDontDelete },
	{"aampSessionID", AAMPJSC_getProperty_aampSessionID, NULL, kJSPropertyAttributeDontDelete },
	{NULL, NULL, NULL, 0}
};

static JSValueRef AAMPJSC_addEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JSController] %s()", __FUNCTION__);
	AAMP_JSController *aampObj = (AAMP_JSController *)JSObjectGetPrivate(thisObject);

	if (aampObj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.addEventListener on instances of AAMP_JSController");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		JSObjectRef callbackFunc = JSValueToObject(context, arguments[1], NULL);

		if (callbackFunc != NULL && JSObjectIsFunction(context, callbackFunc))
		{
			char* type = aamp_JSValueToCString(context, arguments[0], NULL);
			AAMPEventType eventType = aamp_getEventTypeFromName(type);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMPJSCEventListener::AddEventListener(aampObj, eventType, callbackFunc);
			}

			delete[] type;
		}
	}
	else
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.addEventListener' - 2 arguments required");
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeUndefined(context);
}

static JSValueRef AAMPJSC_removeEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JSController] %s()", __FUNCTION__);
	AAMP_JSController *aampObj = (AAMP_JSController *)JSObjectGetPrivate(thisObject);
	JSObjectRef callbackFunc;

	if (aampObj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.removeEventListener on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (aampObj->_eventListeners == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "No event listener registered previously");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		JSObjectRef callbackFunc = JSValueToObject(context, arguments[1], NULL);

		if (callbackFunc != NULL && JSObjectIsFunction(context, callbackFunc))
		{
                        char* type = aamp_JSValueToCString(context, arguments[0], NULL);
                        AAMPEventType eventType = aamp_getEventTypeFromName(type);

                        if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
                        {
                                AAMPJSCEventListener::RemoveEventListener(aampObj, eventType, callbackFunc);
                        }

                        delete[] type;

		}
	}
	else
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.addEventListener' - 2 arguments required");
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeUndefined(context);
}

static const JSStaticFunction AAMP_JSController_static_methods[] =
{
	{"addEventListener", AAMPJSC_addEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{"removeEventListener", AAMPJSC_removeEventListener, kJSPropertyAttributeDontDelete |kJSPropertyAttributeReadOnly },
	{NULL, NULL, 0}
};

void AAMP_JSController_finalize(JSObjectRef thisObj)
{
	LOG("[AAMP_JSController] AAMP_finalize: object=%p\n", thisObj);
	AAMP_JSController *aampObj = (AAMP_JSController*) JSObjectGetPrivate(thisObj);

	if (aampObj == NULL)
		return;

	if (aampObj->_ctx != NULL)
	{
		if (aampObj->_eventListeners != NULL)
		{
			AAMPJSCEventListener::RemoveAllEventListener(aampObj);
		}
	}

	JSObjectSetPrivate(thisObj, NULL);

	delete aampObj;
}

static JSObjectRef AAMP_JSController_class_constructor(JSContextRef context, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	*exception = aamp_GetException(context, AAMPJS_GENERIC_ERROR, "Cannot create an object of AAMP_JSController");
	return NULL;
}

static const JSClassDefinition AAMP_JSController_class_def =
{
	0,
	kJSClassAttributeNone,
	"__AAMPJSController__class",
	NULL,
	AAMP_JSController_static_values,
	AAMP_JSController_static_methods,
	NULL,
	AAMP_JSController_finalize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	AAMP_JSController_class_constructor,
	NULL,
	NULL
};

void aamp_LoadJSController(JSGlobalContextRef context)
{
	INFO("[AAMP_JSController] aamp_LoadJSController context=%p\n", context);

	AAMP_JSController* aampObj = new AAMP_JSController();
	aampObj->_ctx = context;
	aampObj->_aampSessionID = 0;
	aampObj->_eventListeners = NULL;

	_globalController = aampObj;

	aamp_LoadJS(context, NULL);

	JSClassRef classDef = JSClassCreate(&AAMP_JSController_class_def);
	JSObjectRef classObj = JSObjectMake(context, classDef, aampObj);
	JSObjectRef globalObj = JSContextGetGlobalObject(context);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP_JSController");
	JSObjectSetProperty(context, globalObj, str, classObj, kJSPropertyAttributeReadOnly, NULL);
	JSClassRelease(classDef);
	JSStringRelease(str);
}

void aamp_UnloadJSController(JSGlobalContextRef context)
{
	INFO("[AAMP_JSController] aamp_UnloadJSController context=%p\n", context);

	aamp_UnloadJS(context);

	JSObjectRef globalObj = JSContextGetGlobalObject(context);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP_JSController");
	JSValueRef aamp = JSObjectGetProperty(context, globalObj, str, NULL);

	if (aamp == NULL)
	{
		JSStringRelease(str);
		return;
	}

	JSObjectRef aampObj = JSValueToObject(context, aamp, NULL);
	if (aampObj == NULL || JSObjectGetPrivate(aampObj) == NULL)
	{
		JSStringRelease(str);
		return;
	}

	AAMP_JSController_finalize(aampObj);
	_globalController = NULL;

	JSObjectSetProperty(context, globalObj, str, JSValueMakeUndefined(context), kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(str);

	LOG("[AAMP_JSController] JSGarbageCollect(%p)\n", context);
	JSGarbageCollect(context);
}
