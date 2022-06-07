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

/**
 * @file AampDrmHelperFactory.cpp
 * @brief DRM Helper Engine
 */

#include <algorithm>
#include <regex>

#include "AampDrmHelper.h"

/* DRM Helper Engine */

/* Might want to consider double checked here */

/**
 * @brief Get an instance of the DRM Helper Engine
 */
AampDrmHelperEngine& AampDrmHelperEngine::getInstance()
{
	static AampDrmHelperEngine instance;
	return instance;
}

// Consider a mutex for thesie
/**
 * @brief Register a Helper Factory
 */
void AampDrmHelperEngine::registerFactory(AampDrmHelperFactory* factory)
{
	factories.push_back(factory);
	std::sort(factories.begin(), factories.end(),
			  [](AampDrmHelperFactory* a, AampDrmHelperFactory* b) { return (a->getWeighting() < b->getWeighting()); });
}

/**
 * @brief Get the supported OCDM system IDs
 */
void AampDrmHelperEngine::getSystemIds(std::vector<std::string>& ids) const
{
	ids.clear();
	for (auto f : factories )
	{
		f->appendSystemId(ids);
	}
}

/**
 * @brief Build a helper class to support the identified DRM
 */
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
/**
 * @brief AampDrmHelperFactory constructor
 */
AampDrmHelperFactory::AampDrmHelperFactory(int weighting) : mWeighting(weighting)
{
	AampDrmHelperEngine::getInstance().registerFactory(this);
}

/**
 * @brief Determines whether the helper engine has a DRM helper available for the
 * specified DrmInfo
 */
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

