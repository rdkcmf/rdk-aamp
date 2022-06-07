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
 * @file AampMutex.h
 * @brief Helper class for scoped mutex lock
 */

#ifndef _AAMP_MUTEX_H
#define _AAMP_MUTEX_H

#include <pthread.h>

/**
 * @class AampMutexHold 
 * @brief auto Lock the provided mutex during the object scope
 */
class AampMutexHold 
{
	pthread_mutex_t& mutex_;
public:
	AampMutexHold(pthread_mutex_t& mutex): mutex_(mutex)
	{
		pthread_mutex_lock(&mutex_);
	}

	~AampMutexHold()
	{
		pthread_mutex_unlock(&mutex_);
	}
};

#endif //_AAMP_MUTEX_H
