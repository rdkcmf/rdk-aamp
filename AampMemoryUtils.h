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
#ifdef USE_SECMANAGER
#include <sys/shm.h>
#define SHM_ACCESS_PERMISSION 0666
#define SHMGET_RETRY_MAX 10
#endif
/**
 * @struct GrowableBuffer
 * @brief Structure of GrowableBuffer
 */
struct GrowableBuffer
{
	char *ptr;      /**< Pointer to buffer's memory location */
	size_t len;     /**< Buffer size */
	size_t avail;   /**< Available buffer size */
};

/**
 * @fn aamp_Free
 * @param ptr memory allocated with aamp_Malloc
 */
void aamp_Free(void *ptr);
/**
 * @fn aamp_Free
 * @param[in] buffer struct GrowableBuffer
 */
void aamp_Free(struct GrowableBuffer *buffer);
void *aamp_Realloc( void *ptr, size_t numBytes );
void *aamp_Malloc( size_t numBytes );
void aamp_TransferMemory( void *ptr );

/**
 * @brief Allocate memory to growable buffer
 * @param buffer growable buffer
 * @param len size of memory to be allocated
 */
void aamp_Malloc(struct GrowableBuffer *buffer, size_t len);

/**
 * @fn aamp_AppendBytes
 * @param buffer Growable buffer object pointer
 * @param ptr Buffer to append
 * @param len Buffer size
 */
void aamp_AppendBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len);

/**
 * @fn aamp_MoveBytes
 * @param buffer Growable buffer object pointer
 * @param ptr Buffer to Move
 * @param len Buffer size
 */
void aamp_MoveBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len);

/**
 * @fn aamp_AppendNulTerminator
 * @param buffer buffer in which nul to be append
 */
void aamp_AppendNulTerminator(struct GrowableBuffer *buffer);

#ifdef USE_SECMANAGER
/**
 * @fn aamp_CreateSharedMem
 * @param shmLen Length of the buffer to be created
 * @param shmKey shared memory key
 */
void * aamp_CreateSharedMem( size_t shmLen, key_t & shmKey);
/**
 * @fn aamp_CleanUpSharedMem
 * @param shmPointer Pointer to the created memory
 * @param shmKey shared memory key
 * @param shmLen Length of the buffer
 */
void aamp_CleanUpSharedMem(void* shmPointer, key_t shmKey, size_t shmLen);
#endif
#endif /* __AAMP_MEMORY_UTILS_H__ */
