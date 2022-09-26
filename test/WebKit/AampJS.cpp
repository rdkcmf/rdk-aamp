/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <fstream>
#include <sstream>
#include <JavaScriptCore/JavaScript.h>
#include <gst/gst.h>
#include "jsutils.h"

extern "C"
{
	void aamp_LoadJSController(JSGlobalContextRef context);
	void aamp_UnloadJSController(JSGlobalContextRef context);
}

/* AAMP test utilities class method implementations. */
static JSValueRef AAMPTestUtils_JS_sleep(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
static JSValueRef AAMPTestUtils_JS_log(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

/** @brief AAMP test utilities functions */
static JSStaticFunction AAMPTestUtils_JS_static_functions[] {
	{"sleep", AAMPTestUtils_JS_sleep, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{"log", AAMPTestUtils_JS_log, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{NULL, NULL, 0}
};

/** @brief AAMP test utilities class */
static JSClassDefinition AAMPTestUtils_JS_class {
	0, /* version: current (and only) version is 0 */
	kJSClassAttributeNone, /* attributes */
	"__AAMPTestUtils__class", /* className */
	NULL, /* parentClass */
	NULL, /* staticValues */
	AAMPTestUtils_JS_static_functions, /* staticFunctions */
	NULL, /* initialize */
	NULL, /* finalize */
	NULL, /* hasProperty */
	NULL, /* getProperty */
	NULL, /* setProperty */
	NULL, /* deleteProperty */
	NULL, /* getPropertyNames */
	NULL, /* callAsFunction */
	NULL, /* callAsConstructor */
	NULL, /* hasInstance */
	NULL /* convertToType */
};

/** @brief GLib main loop */
static GMainLoop *gMainLoop;

/** @brief GLib main thread */
static GThread *gMainThread;

/**
 * @brief AAMP test utilities sleep
 */
static JSValueRef AAMPTestUtils_JS_sleep(JSContextRef ctx,
										JSObjectRef function,
										JSObjectRef thisObject,
										size_t argumentCount,
										const JSValueRef arguments[],
										JSValueRef *exception)
{
	if (argumentCount == 1)
	{
		if (JSValueIsNumber(ctx, arguments[0]))
		{
			int seconds = JSValueToNumber(ctx, arguments[0], exception);
			(void)sleep(seconds);
		}
		else
		{
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Expected number of seconds as number");
		}
	}
	else
	{
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Expected number of seconds");
	}

	return JSValueMakeUndefined(ctx);
}

/**
 * @brief AAMP test utilities log
 */
static JSValueRef AAMPTestUtils_JS_log(JSContextRef ctx,
									   JSObjectRef function,
									   JSObjectRef thisObject,
									   size_t argumentCount,
									   const JSValueRef arguments[],
									   JSValueRef *exception)
{
	if (argumentCount == 1)
	{
		if (JSValueIsString(ctx, arguments[0]))
		{
			char *string = aamp_JSValueToCString(ctx, arguments[0], exception);
			if (NULL != string)
			{
				std::cout << string << std::endl;
				SAFE_DELETE_ARRAY(string);
			}
		}
		else
		{
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Expected log string");
		}
	}
	else
	{
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Expected log string");
	}

	return JSValueMakeUndefined(ctx);
}

/**
 * @brief GLib thread
 */
static void *glibThread(void *arg)
{
	g_main_loop_run(gMainLoop);
	g_main_loop_unref(gMainLoop);
	return NULL;
}

/**
 * @brief Read JavaScript file
 *
 * @param[in] name Filename
 * @param[out] script JavaScript
 * @retval 0 on success
 * @retval 1 on error
 */
static int ReadJavaScriptFile(const std::string &name,
							  std::string &script)
{
	int rv;
	std::ifstream inFile;

	inFile.open(name);
	if (!inFile)
	{
		std::cerr << "Failed to open " << name << std::endl;
		rv = 1;
	}
	else
	{
		std::stringstream strStream;
		strStream << inFile.rdbuf();
		script = strStream.str();
		rv = 0;
	}

	return rv;
}

/**
 * @brief Run JavaScript
 *
 * @param[in] context JavaScript context
 * @param[in] script JavaScript
 * @param[in] sourceURL Source URL or empty string
 * @retval 0 on success
 * @retval 1 on error
 */
static int RunJavaScript(JSContextRef context,
						 const std::string &script,
						 const std::string &sourceURL)
{
	int rv = 1;
	JSStringRef jScript = NULL;
	JSStringRef jSourceURL = NULL;
	JSValueRef jResult = NULL;

	/* Create optional source URL JavaScript string. */
	if (!sourceURL.empty())
	{
		jSourceURL = JSStringCreateWithUTF8CString(sourceURL.c_str());
		if (NULL == jSourceURL)
		{
			std::cerr << "Failed to create source URL string" << std::endl;
		}
	}

	/* Create script JavaScript string. */
	jScript = JSStringCreateWithUTF8CString(script.c_str());
	if (NULL == jScript)
	{
		std::cerr << "Failed to create script string" << std::endl;
	}
	else
	{
		jResult = JSEvaluateScript(context, jScript, NULL, jSourceURL, 1, NULL);
		if (jResult == NULL)
		{
			std::cerr << "Exception thrown" << std::endl;
		}
		else
		{
			rv = 0;
		}
	}

	if (NULL != jScript)
	{
		JSStringRelease(jScript);
	}

	if (NULL != jSourceURL)
	{
		JSStringRelease(jSourceURL);
	}

	if (NULL != jResult)
	{
		JSValueUnprotect(context, jResult);
	}

	return rv;
}

/**
 * @brief Setup AAMP test utilities JavaScript class
 *
 * @param[in] context JavaScript context
 * @retval 0 on success
 * @retval 1 on error
 */
static int SetupAampTestUtilsClass(JSContextRef context)
{
	int rv = 1;
	JSObjectRef globalObj;
	JSClassRef testClass;
	JSObjectRef classObj = NULL;
	JSStringRef jstr;

	globalObj = JSContextGetGlobalObject(context);
	jstr = JSStringCreateWithUTF8CString("AAMPTestUtils");
	testClass = JSClassCreate(&AAMPTestUtils_JS_class);
	if (NULL == globalObj)
	{
		std::cerr << "Failed to get global object" << std::endl;
	}
	else if (NULL == jstr)
	{
		std::cerr << "Failed to create AAMPTestUtils string" << std::endl;
	}
	else if (NULL == testClass)
	{
		std::cerr << "Failed to create AAMPTestUtils class" << std::endl;
	}
	else
	{
		/* Create and protect the AAMP test utilities class constructor. */
		classObj = JSObjectMakeConstructor(context, testClass, NULL);
		if (NULL == classObj)
		{
			std::cerr << "Failed to create AAMPTestUtils class object" << std::endl;
		}
		else
		{
			JSValueProtect(context, classObj);
			JSObjectSetProperty(context, globalObj, jstr, classObj, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);
			rv = 0;
		}
	}

	if (NULL != testClass)
	{
		JSClassRelease(testClass);
	}

	if (NULL != jstr)
	{
		JSStringRelease(jstr);
	}

	return rv;
}

/**
 * @brief Remove AAMP test utilities JavaScript class
 *
 * @param[in] context JavaScript context
 * @retval 0 on success
 * @retval 1 on error
 */
static int RemoveAampTestUtilsClass(JSContextRef context)
{
	int rv = 1;
	JSObjectRef globalObj;
	JSValueRef classObj = NULL;
	JSStringRef jstr;

	globalObj = JSContextGetGlobalObject(context);
	jstr = JSStringCreateWithUTF8CString("AAMPTestUtils");
	if (NULL == globalObj)
	{
		std::cerr << "Failed to get global object" << std::endl;
	}
	else if (NULL == jstr)
	{
		std::cerr << "Failed to create AAMPTestUtils string" << std::endl;
	}
	else
	{
		/* Remove and unprotect the AAMP test utilities class constructor. */
		classObj = JSObjectGetProperty(context, globalObj, jstr, NULL);
		if (NULL == classObj)
		{
			std::cerr << "Failed to get AAMPTestUtils class object" << std::endl;
		}
		else
		{
			JSValueUnprotect(context, classObj);
			JSObjectSetProperty(context, globalObj, jstr, JSValueMakeUndefined(context), kJSPropertyAttributeReadOnly, NULL);
			rv = 0;
		}
	}

	if (NULL != jstr)
	{
		JSStringRelease(jstr);
	}

	return rv;
}

/**
 * @brief Main function
 */
int main(int argc,
		 char *argv[])
{
	int exitCode;

	/* Display a usage message if the first argument starts with '-' but is not
	 * "--".
	 */
	if ((argc > 1) && (argv[1][0] == '-') && ((argv[1][1] != '-') || ((argv[1][1] == '-') && (argv[1][2] != '\0'))))
	{
		std::cerr << "AAMP JavaScript test utility" << std::endl;
		std::cerr << "Usage: " << argv[0] << " [--] [file.js] ..." << std::endl;
		exit(0);
	}

	/* Initialise GLib. */
	gst_init(NULL, NULL);
	gMainLoop = g_main_loop_new(NULL, FALSE);
	gMainThread = g_thread_new("AAMPPlayerLoop", &glibThread, NULL);

	/* Create JavaScript global context. */
	JSGlobalContextRef globalContext = JSGlobalContextCreate(NULL);

	/* Setup AAMP JavaScript bindings. */
	aamp_LoadJSController(globalContext);

	/* Setup AAMP test utilities Javascript class. */
	exitCode = SetupAampTestUtilsClass((JSContextRef)globalContext);

	for (int i = 1; (i < argc) && (0 == exitCode); i++)
	{
		std::string name(argv[i]);

		if (name != "--")
		{
			std::string script;

			/* Read JavaScript file. */
			exitCode = ReadJavaScriptFile(name, script);
			if (exitCode == 0)
			{
				/* Execute JavaScript. */
				exitCode = RunJavaScript((JSContextRef)globalContext, script, name);
			}
		}
	}

	/* Cleanup JavaScript resources. */
	(void)RemoveAampTestUtilsClass(globalContext);
	aamp_UnloadJSController(globalContext);
	JSGlobalContextRelease(globalContext);

	exit(exitCode);
}
