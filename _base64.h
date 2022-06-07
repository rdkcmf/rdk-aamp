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

#ifndef BASE64_H
#define BASE64_H

/**
 * @file _base64.h
 * @brief base64 source Encoder/Decoder
 */

#include <stddef.h>

/**
 * @fn base64_Encode
 * @param src pointer to first byte of binary data to be encoded
 * @param len number of bytes to encode
 */
char *base64_Encode(const unsigned char *src, size_t len);

/**
 * @fn base64_Decode
 * @param src pointer to cstring containing base64-encoded data
 * @param len receives byte length of returned pointer, or zero upon failure
 */
unsigned char *base64_Decode(const char *src, size_t *len);

/**
 * @fn base64_Decode
 * @param src pointer to cstring containing base64-encoded data
 * @param len receives byte length of returned pointer, or zero upon failure
 * @param srcLen string length of src
 */
unsigned char *base64_Decode(const char *src, size_t *len, size_t srcLen);

#endif // BASE64_H
