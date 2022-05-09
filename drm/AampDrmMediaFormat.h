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

#ifndef AAMPDRMMEDIAFORMAT_H
#define AAMPDRMMEDIAFORMAT_H

/** 
 * @file AampDrmMediaFormat.h
 * @brief Types of Media 
 */

/**
 * @enum MediaFormat
 * @brief Media format types
 */
typedef enum
{
	eMEDIAFORMAT_HLS,		   /**< HLS Media */
	eMEDIAFORMAT_DASH,		   /**< Dash Media */
	eMEDIAFORMAT_PROGRESSIVE,	   /**< Progressive Media */
	eMEDIAFORMAT_HLS_MP4,		   /**< HLS mp4 Format */
	eMEDIAFORMAT_OTA,		   /**< OTA Media */
	eMEDIAFORMAT_HDMI,		   /**< HDMI Format */
	eMEDIAFORMAT_COMPOSITE,		   /**< Composite Media */
	eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA, /**< Smooth streaming Media */
	eMEDIAFORMAT_UNKNOWN		   /**< Unknown media format */
} MediaFormat;

#endif /* AAMPDRMMEDIAFORMAT_H */

