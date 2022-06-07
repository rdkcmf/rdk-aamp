/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file AampGstUtils.h
 * @brief Header for utility functions for AAMP's GST impl
 */

#ifndef __AAMP_GST_UTILS_H__
#define __AAMP_GST_UTILS_H__

#include <gst/gst.h>
#include "main_aamp.h"

/**
 * @fn GetGstCaps
 * @param[in] format stream format to generate caps
 * @retval GstCaps for the input format
 */
GstCaps* GetGstCaps(StreamOutputFormat format);

#endif /* __AAMP_GST_UTILS_H__ */
