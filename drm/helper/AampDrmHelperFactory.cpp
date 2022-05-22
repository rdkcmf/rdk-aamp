/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include <algorithm>
#include <regex>

#include "AampDrmHelper.h"

/* DRM Helper Engine */

/* Might want to consider double checked here */
AampDrmHelperEngine& AampDrmHelperEngine::getInstance()
{
	static AampDrmHelperEngine instance;
	return instance;
}

// Consider a mutex for these
void AampDrmHelperEngine::registerFactory(AampDrmHelperFactory* factory)
{
	factories.push_back(factory);
	std::sort(factories.begin(), factories.end(),
			  [](AampDrmHelperFactory* a, AampDrmHelperFactory* b) { return (a->getWeighting() < b->getWeighting()); });
}

void AampDrmHelperEngine::getSystemIds(std::vector<std::string>& ids) const
{
	ids.clear();
	for (auto f : factories )
	{
		f->appendSystemId(ids);
	}
}

std::shared_ptr<AampDrmHelper> AampDrmHelperEngine::createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj) const
{
	for (auto helper : factories)
	{
		if (true == helper->isDRM(drmInfo))
		{
			return helper->createHelper(drmInfo, logObj);
		}
	}

	return NULL;
}

/* DRM Helper Factory */

AampDrmHelperFactory::AampDrmHelperFactory(int weighting) : mWeighting(weighting)
{
	AampDrmHelperEngine::getInstance().registerFactory(this);
}

bool AampDrmHelperEngine::hasDRM(const struct DrmInfo& drmInfo) const
{
	for (auto helper : factories)
	{
		if (true == helper->isDRM(drmInfo))
		{
			return true;
		}
	}

	return false;
}

