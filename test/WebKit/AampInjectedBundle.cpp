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

#include <string>
#include <gst/gst.h>
#include "cJSON.h"
#include "jsutils.h"
#include <WebKit/WKType.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKString.h>
#include <WKBundleInitialize.h>
#include <WKBundle.h>
#include <WKBundleFrame.h>
#include <WKBundlePage.h>
#include <WKBundlePageLoaderClient.h>

extern "C"
{
	void aamp_LoadJSController(JSGlobalContextRef context);
	void aamp_UnloadJSController(JSGlobalContextRef context);
	void aamp_SetPageHttpHeaders(const char* headers);
}

/**
 * @brief Did load page
 */
void didCommitLoadForFrame(WKBundlePageRef page,
						   WKBundleFrameRef frame,
						   WKTypeRef *userData,
						   const void *clientInfo)
{
	if (WKBundlePageGetMainFrame(page) == frame)
	{
		/* Load AAMP JavaScript controller. */
		JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
		INFO("Loading JavaScript controller");
		aamp_LoadJSController(context);
	}
}

/**
 * @brief Did start new page
 */
void didStartProvisionalLoadForFrame(WKBundlePageRef page,
									 WKBundleFrameRef frame,
									 WKTypeRef *userData,
									 const void *clientInfo)
{
	WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
	if (mainFrame == frame)
	{
		/* Unload AAMP JavaScript controller (if any). */
		JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
		INFO("Unloading JavaScript controller");
		aamp_UnloadJSController(context);
	}
}

/**
 * @brief Did create page
 */
void didCreatePage(WKBundleRef,
				   WKBundlePageRef page,
				   const void* clientInfo)
{
	WKBundlePageLoaderClientV1 client {
		{1, clientInfo},

		// Version 0.
		didStartProvisionalLoadForFrame,
		nullptr, // didReceiveServerRedirectForProvisionalLoadForFrame;
		nullptr, // didFailProvisionalLoadWithErrorForFrame;
		didCommitLoadForFrame,
		nullptr, // didFinishDocumentLoadForFrame;
		nullptr, // didFinishLoadForFrame;
		nullptr, // didFailLoadWithErrorForFrame;
		nullptr, // didSameDocumentNavigationForFrame;
		nullptr, // didReceiveTitleForFrame;
		nullptr, // didFirstLayoutForFrame;
		nullptr, // didFirstVisuallyNonEmptyLayoutForFrame;
		nullptr, // didRemoveFrameFromHierarchy;
		nullptr, // didDisplayInsecureContentForFrame;
		nullptr, // didRunInsecureContentForFrame;
		nullptr, // didClearWindowObjectForFrame;
		nullptr, // didCancelClientRedirectForFrame;
		nullptr, // willPerformClientRedirectForFrame;
		nullptr, // didHandleOnloadEventsForFrame,

		// Version 1.
		nullptr, // didLayoutForFrame
		nullptr, // didNewFirstVisuallyNonEmptyLayout_unavailable
		nullptr, // didDetectXSSForFrame
		nullptr, // shouldGoToBackForwardListItem,
		nullptr, // globalObjectIsAvailableForFrame
		nullptr, // willDisconnectDOMWindowExtensionFromGlobalObject
		nullptr, // didReconnectDOMWindowExtensionToGlobalObject
		nullptr // willDestroyGlobalObjectForDOMWindowExtension
	};

	INFO("Setting injection bundle page loader client");
	WKBundlePageSetPageLoaderClient(page, &client.base);
}

/**
 * @brief About to destroy page
 */
void willDestroyPage(WKBundleRef bundle,
					 WKBundlePageRef page,
					 const void* clientInfo)
{
	WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
	if (mainFrame != NULL)
	{
		/* Unload AAMP JavaScript controller. */
		JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
		INFO("Unloading JavaScript controller");
		aamp_UnloadJSController(context);
	}
}

/**
 * @brief Initialize WebKit Injected Bundle
 */
extern "C" void WKBundleInitialize(WKBundleRef bundle,
								   WKTypeRef initializationUserData)
{
	WKBundleClientV1 client = {
		{1, nullptr},

		// Version 0.
		didCreatePage,
		willDestroyPage,
		nullptr, // didInitializePageGroup
		nullptr, // didReceiveMessage

		// Version 1.
		nullptr // didReceiveMessageToPage
	};

	/* Ensure that GStreamer is initialised. */
	gst_init(NULL, NULL);

	/* Setup the injected bundle. */
	INFO("Setting injection bundle client");
	WKBundleSetClient(bundle, &client.base);
}
