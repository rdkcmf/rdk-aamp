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

#ifndef AAMP_IARM_MGR_MOCKS_H
#define AAMP_IARM_MGR_MOCKS_H

#include "libIBus.h"
#include "host.hpp"
#include "videoOutputPort.hpp"
#include "pixelResolution.hpp"
#include "manager.hpp"
#include <gmock/gmock.h>

using namespace device;

/* IARM Bus library mock. */
class AampMockIarmBus
{
	public:
		MOCK_METHOD(IARM_Result_t, IARM_Bus_Call, (const char *ownerName,
												   const char *methodName,
												   void *arg,
												   size_t argLen));

		MOCK_METHOD(IARM_Result_t, IARM_Bus_Connect, ());

		MOCK_METHOD(IARM_Result_t, IARM_Bus_Init, (const char *name));

		MOCK_METHOD(IARM_Result_t, IARM_Bus_RegisterEventHandler, (const char *ownerName,
																   IARM_EventId_t eventId,
																   IARM_EventHandler_t handler));

		MOCK_METHOD(IARM_Result_t, IARM_Bus_RemoveEventHandler, (const char *ownerName,
																 IARM_EventId_t eventId,
																 IARM_EventHandler_t handler));
};

extern AampMockIarmBus *g_mockIarmBus;

/* Device settings library mocks. */
class AampMockDeviceHost
{
	public:
		MOCK_METHOD(VideoOutputPort &, getVideoOutputPort, (const std::string &name));
};

class AampMockDeviceManager
{
	public:
		MOCK_METHOD(void, DeInitialize, ());
		MOCK_METHOD(void, Initialize, ());
};

class AampMockDeviceVideoOutputPort
{
	public:
		MOCK_METHOD(int, getHDCPCurrentProtocol, ());
		MOCK_METHOD(int, getHDCPProtocol, ());
		MOCK_METHOD(int, getHDCPReceiverProtocol, ());
		MOCK_METHOD(int, getHDCPStatus, ());
		MOCK_METHOD(const VideoResolution &, getResolution, ());
		MOCK_METHOD(bool, isContentProtected, (), (const));
		MOCK_METHOD(bool, isDisplayConnected, (), (const));
};

class AampMockDevicePixelResolution
{
	public:
		MOCK_METHOD(const PixelResolution &, getPixelResolution, (), (const));
};

extern AampMockDeviceHost *g_mockDeviceHost;
extern AampMockDeviceManager *g_mockDeviceManager;
extern AampMockDeviceVideoOutputPort *g_mockVideoOutputPort;

#endif /* AAMP_IARM_MGR_MOCKS_H */
