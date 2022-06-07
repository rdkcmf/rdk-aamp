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

#ifndef BASE16_H
#define BASE16_H

/**
 * @file base16.h
 * @brief optimized way way base16 Encode/Decode operation
 */

#include <stddef.h>


/**
 * @fn base16_Encode
 * @param src pointer to first byte of binary data to be encoded
 * @param len number of bytes to encode
 */
char *base16_Encode(const unsigned char *src, size_t len);


/**
 * @fn base16_Decode
 * @param srcPtr pointer to cstring containing base16-encoded data
 * @param srcLen length of srcPtr (typically caller already knows, so saves call to strlen)
 * @param len receives byte length of returned pointer, or zero upon failure
 */
unsigned char *base16_Decode(const char *srcPtr, size_t srcLen, size_t *len);

#endif // BASE16_H
