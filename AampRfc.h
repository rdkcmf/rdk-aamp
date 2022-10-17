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
 * @file AampRfc.h
 * @brief Aamp RFC header files
 */
#ifndef _AAMP_RFC_H
#define _AAMP_RFC_H

#ifdef AAMP_RFC_ENABLED
namespace RFCSettings{ 
    /**
     * @fn getLHRAcceptValue
     * @retval  std::string host value
     */
    std::string getLHRAcceptValue();

    /**
     * @fn getLRHContentType   
     * @retval  std::string host value
     */
    std::string getLRHContentType();

    /**
     * @fn getSchemeIdUriDaiStream
     * @retval  std::string scheme id uri
     */
    std::string getSchemeIdUriDaiStream();

    /**
     * @fn getSchemeIdUriVssStream
     * @retval  std::string scheme id uri
     */
    std::string getSchemeIdUriVssStream();

    /**
     * @fn getRFCValue
     * @brief   Fetch data from RFC
     * @param   Parameter to be fetched
     * @retval  std::string host value
     */
    std::string getRFCValue(const std::string& parameter);
}
#endif
#endif
