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
* @file AampDrmSession.cpp
* @brief Source file for AampDrmSession.
*/

#include "AampDrmSession.h"

/**
 *  @brief	AampDrmSession constructor.
 */
AampDrmSession::AampDrmSession(const string &keySystem) : m_keySystem(keySystem),m_OutputProtectionEnabled(false)
{
}

/**
 *  @brief	AampDrmSession destructor.
 */
AampDrmSession::~AampDrmSession()
{
}

/**
 *  @brief	Getter function for DRM key system.
 *
 *  @return	keySystem, com.microsoft.playready or com.widevine.alpha
 */
string AampDrmSession::getKeySystem()
{
	return m_keySystem;
}

int AampDrmSession::decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps)
{
	AAMPLOG_ERR("GST decrypt method not implemented");
	return -1;
}

int AampDrmSession::decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData)
{
	AAMPLOG_ERR("Standard decrypt method not implemented");
	return -1;
}
