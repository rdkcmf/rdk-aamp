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

#ifndef AAMPMEDIATYPE_H
#define AAMPMEDIATYPE_H

/**
 * @brief Media types
 */
// Please maintain the order video, audio, subtitle and aux_audio in future
// Above order to be maintained across fragment, init and playlist media types
// These enums are used in a lot of calculation in AAMP code and breaking the order will bring a lot of issues
// This order is also followed in other enums like AampCurlInstance and TrackType
enum MediaType
{
	eMEDIATYPE_VIDEO,               /**< Type video */
	eMEDIATYPE_AUDIO,               /**< Type audio */
	eMEDIATYPE_SUBTITLE,            /**< Type subtitle */
	eMEDIATYPE_AUX_AUDIO,           /**< Type auxiliary audio */
	eMEDIATYPE_MANIFEST,            /**< Type manifest */
	eMEDIATYPE_LICENCE,             /**< Type license */
	eMEDIATYPE_IFRAME,              /**< Type iframe */
	eMEDIATYPE_INIT_VIDEO,          /**< Type video init fragment */
	eMEDIATYPE_INIT_AUDIO,          /**< Type audio init fragment */
	eMEDIATYPE_INIT_SUBTITLE,       /**< Type subtitle init fragment */
	eMEDIATYPE_INIT_AUX_AUDIO,      /**< Type auxiliary audio init fragment */
	eMEDIATYPE_PLAYLIST_VIDEO,      /**< Type video playlist */
	eMEDIATYPE_PLAYLIST_AUDIO,      /**< Type audio playlist */
	eMEDIATYPE_PLAYLIST_SUBTITLE,	/**< Type subtitle playlist */
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,	/**< Type auxiliary audio playlist */
	eMEDIATYPE_PLAYLIST_IFRAME,     /**< Type Iframe playlist */
	eMEDIATYPE_INIT_IFRAME,         /**< Type IFRAME init fragment */
	eMEDIATYPE_DSM_CC,              /**< Type digital storage media command and control (DSM-CC) */
	eMEDIATYPE_IMAGE,		/**< Type image for thumbnail playlist */
	eMEDIATYPE_DEFAULT              /**< Type unknown */
};


#endif /* AAMPMEDIATYPE_H */

