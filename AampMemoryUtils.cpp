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
 * @file AampMemoryUtils.cpp
 * @brief Helper functions for memory management
 */

#include "AampMemoryUtils.h"
#include "AampConfig.h"
#include <assert.h>
#include <glib.h>
#include <errno.h>

/**
 * @brief Free memory allocated by aamp_Malloc
 * @param[in][out] struct GrowableBuffer, this structure has ptr variable allocated by aamp.
 */
void aamp_Free(struct GrowableBuffer *buffer)
{
	if (buffer && buffer->ptr)
	{
		g_free(buffer->ptr);
		buffer->ptr = NULL;
	}
}



/**
 * @brief Append data to buffer
 * @param buffer Growable buffer object pointer
 * @param ptr Buffer to append
 * @param len Buffer size
 */
void aamp_AppendBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len)
{
	size_t required = buffer->len + len;
	if (required > buffer->avail)
	{
		// For encoded contents, step by step increment 512KB => 1MB => 2MB => ..
		// In other cases, double the buffer based on availability and requirement.
		buffer->avail = ((buffer->avail * 2) > required) ? (buffer->avail * 2) : (required * 2);
		char *ptr = (char *)g_realloc(buffer->ptr, buffer->avail);
		assert(ptr);
		if (ptr)
		{
			if (buffer->ptr == NULL)
			{ // first alloc (not a realloc)
			}
			buffer->ptr = ptr;
		}
	}
	if (buffer->ptr)
	{
		memcpy(&buffer->ptr[buffer->len], ptr, len);
		buffer->len = required;
	}
}

/**
 * @brief Move data to buffer
 * @param buffer Growable buffer object pointer
 * @param ptr Buffer to Move
 * @param len Buffer size
 */
void aamp_MoveBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len)
{
	/* remove parsed data from memory */
	if (buffer->ptr && ptr)
	{
        if(buffer->avail >= len)
        {
            memmove(&buffer->ptr[0], ptr, len);
            buffer->len = len;
        }
        else
        {
            AAMPLOG_WARN("WARNING - Something is wrong!! Trying to move more than available!!!");
        }
	}
	else
	{
		AAMPLOG_WARN("WARNING - NULL pointer input!!!");
	}
}

/**
 * @brief Append nul character to buffer
 * @param buffer buffer in which nul to be append
 */
void aamp_AppendNulTerminator(struct GrowableBuffer *buffer)
{
	char zeros[2] = { 0, 0 }; // append two bytes, to distinguish between internal inserted 0x00's and a final 0x00 0x00
	aamp_AppendBytes(buffer, zeros, 2);
}

#ifdef USE_SECMANAGER

/**
 * @brief Createa share memory and provide the key
 * @param shmPointer Pointer to the created memory
 * @param shmLen Length of the buffer to be created
 * @param shmKey shared memory key
 */
void * aamp_CreateSharedMem( size_t shmLen, key_t & shmKey)
{
	void *shmPointer = NULL;
	if( shmLen > 0)
	{
		for( int retryCount=0; retryCount<SHMGET_RETRY_MAX; retryCount++ )
		{
			// generate pseudo-random value to use as a unique key
			shmKey = rand() + 1; // must be non-zero to allow access to non-child processes
			
			// allocate memory segment and identifier
			int shmId = shmget(shmKey, shmLen,
							   IPC_CREAT | // create new segment
							   IPC_EXCL | // fail if already exists
							   SHM_ACCESS_PERMISSION); // owner, group, other permissions
			if (shmId != -1 )
			{ // map newly allocated segment to shared memory pointer
				shmPointer = shmat(shmId, NULL, 0 );
				if( shmPointer != (void *)-1 )
				{ // success!
					AAMPLOG_WARN("%s:%d Shared memory shmId=%d ptr=%p created for the key, %u\n",
								 __FUNCTION__, __LINE__, shmId, shmPointer, shmKey);
				}
				else
				{
					AAMPLOG_ERR("%s:%d shmat err=%d shmId=%d\n", __FUNCTION__, __LINE__, errno, shmId );
					aamp_CleanUpSharedMem( shmPointer, shmKey, shmLen);
					shmPointer = NULL;
				}
				break;
			}
			else
			{
				AAMPLOG_ERR("%s:%d shmget err=%d", __FUNCTION__, __LINE__, errno);
			}
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d invalid shmLen=%zu", __FUNCTION__, __LINE__, shmLen);
	}
	return shmPointer;
}


/**
 * @brief Detatch and delete shared memory
 * @param shmPointer Pointer to the created memory
 * @param shmKey shared memory key
 * @param shmLen Length of the buffer
 */
void aamp_CleanUpSharedMem(void* shmPointer, key_t shmKey, size_t shmLen)
{
	if( NULL != shmPointer && (void*)-1 != shmPointer)
	{ // detach shared memory
		if( -1 == shmdt(shmPointer) )
		{
			AAMPLOG_ERR("%s:%d shmdt failure %d for key %u \n", __FUNCTION__, __LINE__, errno, shmKey);
		}
		int shmId = shmget(shmKey, shmLen, SHM_ACCESS_PERMISSION);
		if( shmId != -1 )
		{ // mark segment to be destroyed
			if( -1 == shmctl(shmId, IPC_RMID, NULL) )
			{
				AAMPLOG_ERR("%s:%d shmctl err=%d shmId=%d\n", __FUNCTION__, __LINE__, errno, shmId );
			}
		}
		else
		{
			AAMPLOG_ERR("%s:%d  bad shmKey=%u\n", __FUNCTION__, __LINE__, shmKey);
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d bad shmPointer=%p\n", __FUNCTION__, __LINE__, shmPointer );
	}
}

#endif
