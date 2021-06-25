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
 * @file AampMemoryUtils.h
 * @brief Header file of helper functions for memory management
 */


#ifndef __AAMP_MEMORY_UTILS_H__
#define __AAMP_MEMORY_UTILS_H__

#include <stddef.h>

/**
 * @brief Structure of GrowableBuffer
 */
struct GrowableBuffer
{
	char *ptr;      /**< Pointer to buffer's memory location */
	size_t len;     /**< Buffer size */
	size_t avail;   /**< Available buffer size */
};

/**
 * @brief Free memory allocated by aamp_Malloc
 * @param[in][out] struct GrowableBuffer
 */
void aamp_Free(struct GrowableBuffer *buffer);

/**
 * @brief Allocate memory to growable buffer
 * @param buffer growable buffer
 * @param len size of memory to be allocated
 */
void aamp_Malloc(struct GrowableBuffer *buffer, size_t len);

/**
 * @brief Append data to buffer
 * @param buffer Growable buffer object pointer
 * @param ptr Buffer to append
 * @param len Buffer size
 */
void aamp_AppendBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len);

/**
 * @brief Move data to buffer
 * @param buffer Growable buffer object pointer
 * @param ptr Buffer to Move
 * @param len Buffer size
 */
void aamp_MoveBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len);

/**
 * @brief Append nul character to buffer
 * @param buffer buffer in which nul to be append
 */
void aamp_AppendNulTerminator(struct GrowableBuffer *buffer);

#endif /* __AAMP_MEMORY_UTILS_H__ */
