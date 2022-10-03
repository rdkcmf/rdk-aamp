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

#include "AampEventManager.h"
#include "MockAampEventManager.h"

MockAampEventManager *g_mockAampEventManager = nullptr;

AampEventManager::AampEventManager(AampLogManager *logObj)
{
}

AampEventManager::~AampEventManager()
{
}

void AampEventManager::AddListenerForAllEvents(EventListener* eventListener)
{
}

void AampEventManager::RemoveListenerForAllEvents(EventListener* eventListener)
{
}

void AampEventManager::SendEvent(const AAMPEventPtr &eventData, AAMPEventMode eventMode)
{
    if (g_mockAampEventManager != nullptr)
    {
        g_mockAampEventManager->SendEvent(eventData, eventMode);
    }
}

void AampEventManager::FlushPendingEvents()
{
}

void AampEventManager::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
}

void AampEventManager::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
}

bool AampEventManager::IsEventListenerAvailable(AAMPEventType eventType)
{
    if (g_mockAampEventManager != nullptr)
    {
        return g_mockAampEventManager->IsEventListenerAvailable(eventType);
    }
    else
    {
        return false;
    }
}

void AampEventManager::SetFakeTuneFlag(bool isFakeTuneSetting)
{
}

void AampEventManager::SetAsyncTuneState(bool isAsyncTuneSetting)
{
}

void AampEventManager::SetPlayerState(PrivAAMPState state)
{
}

bool AampEventManager::IsSpecificEventListenerAvailable(AAMPEventType eventType)
{	
    return false;
}