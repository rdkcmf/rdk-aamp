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

#include "MockAampScheduler.h"
#include "AampScheduler.h"

MockAampScheduler *g_mockAampScheduler = nullptr;

AampScheduler::AampScheduler()
{
}

AampScheduler::~AampScheduler()
{
}

void AampScheduler::StartScheduler()
{
}

void AampScheduler::StopScheduler()
{
}

void AampScheduler::SuspendScheduler()
{
}

void AampScheduler::ResumeScheduler()
{
}

void AampScheduler::RemoveAllTasks()
{
}

int AampScheduler::ScheduleTask(AsyncTaskObj obj)
{
    if (g_mockAampScheduler != nullptr)
    {
        return g_mockAampScheduler->ScheduleTask(obj);
    }
    else
    {
        return 0;
    }
}

void AampScheduler::SetState(PrivAAMPState sstate)
{
    if (g_mockAampScheduler != nullptr)
    {
        return g_mockAampScheduler->SetState(sstate);
    }
}

bool AampScheduler::RemoveTask(int id)
{
	return false;
}
