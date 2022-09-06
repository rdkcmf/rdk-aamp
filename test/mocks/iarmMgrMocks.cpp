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

#include "iarmMgrMocks.h"

/* IARM Bus library mock. */
AampMockIarmBus *g_mockIarmBus;

IARM_Result_t IARM_Bus_Call(const char *ownerName,
							const char *methodName,
							void *arg,
							size_t argLen)
{
	return g_mockIarmBus->IARM_Bus_Call(ownerName, methodName, arg, argLen);
}

IARM_Result_t IARM_Bus_Connect(void)
{
	return g_mockIarmBus->IARM_Bus_Connect();
}

IARM_Result_t IARM_Bus_Init(const char *name)
{
	return g_mockIarmBus->IARM_Bus_Init(name);
}

IARM_Result_t IARM_Bus_RegisterEventHandler(const char *ownerName,
											IARM_EventId_t eventId,
											IARM_EventHandler_t handler)
{
	return g_mockIarmBus->IARM_Bus_RegisterEventHandler(ownerName, eventId, handler);
}

IARM_Result_t IARM_Bus_RemoveEventHandler(const char *ownerName,
										  IARM_EventId_t eventId,
										  IARM_EventHandler_t handler)
{
	return g_mockIarmBus->IARM_Bus_RemoveEventHandler(ownerName, eventId, handler);
}

/* Device settings library mocks. */
AampMockDeviceHost *g_mockDeviceHost;
AampMockDeviceManager *g_mockDeviceManager;
AampMockDeviceVideoOutputPort *g_mockDeviceVideoOutputPort;

namespace device
{
Host::Host()
{
}

Host::~Host()
{
}

Host &Host::getInstance(void)
{
	static Host instance;
	return instance;
}

VideoOutputPort &Host::getVideoOutputPort(const std::string &name)
{
	return g_mockDeviceHost->getVideoOutputPort(name);
}

void Manager::DeInitialize()
{
	g_mockDeviceManager->DeInitialize();
}

void Manager::Initialize()
{
	g_mockDeviceManager->Initialize();
}

PixelResolution::PixelResolution(int id)
{
}

PixelResolution::~PixelResolution()
{
}

const int PixelResolution::k720x480 = 0;
const int PixelResolution::k720x576 = 1;
const int PixelResolution::k1280x720 = 2;
const int PixelResolution::k1920x1080 = 3;
const int PixelResolution::k3840x2160 = 4;
const int PixelResolution::k4096x2160 = 5;

VideoOutputPort::VideoOutputPort(const int type,
								 const int index,
								 const int id,
								 int audioPortId,
								 const std::string &resolution)
{
}

VideoOutputPort::~VideoOutputPort()
{
}

VideoOutputPort::Display::Display(VideoOutputPort &vPort)
{
}

VideoOutputPort::Display::~Display()
{
}

int VideoOutputPort::getHDCPCurrentProtocol()
{
	return g_mockDeviceVideoOutputPort->getHDCPCurrentProtocol();
}

int VideoOutputPort::getHDCPProtocol()
{
	return g_mockDeviceVideoOutputPort->getHDCPProtocol();
}

int VideoOutputPort::getHDCPReceiverProtocol()
{
	return g_mockDeviceVideoOutputPort->getHDCPReceiverProtocol();
}

int VideoOutputPort::getHDCPStatus()
{
	return g_mockDeviceVideoOutputPort->getHDCPStatus();
}

const VideoResolution &VideoOutputPort::getResolution()
{
	return g_mockDeviceVideoOutputPort->getResolution();
}

bool VideoOutputPort::isContentProtected() const
{
	return g_mockDeviceVideoOutputPort->isContentProtected();
}

bool VideoOutputPort::isDisplayConnected() const
{
	return g_mockDeviceVideoOutputPort->isDisplayConnected();
}

VideoResolution::VideoResolution(const int id,
								 const std::string &name,
								 int resolutionId,
								 int ratioid,
								 int ssModeId,
								 int frameRateId,
								 bool interlacedId,
								 bool enabled)
{
}

VideoResolution::~VideoResolution()
{
}

const PixelResolution &VideoResolution::getPixelResolution() const
{
	static PixelResolution instance(0);
	return instance;
}
}
