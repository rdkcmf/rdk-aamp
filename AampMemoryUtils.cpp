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
#include "GlobalConfigAAMP.h" //Required for logprintf, TODO: fix this
#include <assert.h>
#include <glib.h>

/**
 * @brief Free memory allocated by aamp_Malloc
 * @param[in][out] pptr Pointer to allocated memory
 */
void aamp_Free(char **pptr)
{
	void *ptr = *pptr;
	if (ptr)
	{
		g_free(ptr);
		*pptr = NULL;
	}
}


/**
 * @brief Allocate memory to growable buffer
 * @param buffer growable buffer
 * @param len size of memory to be allocated
 */
void aamp_Malloc(struct GrowableBuffer *buffer, size_t len)
{
	assert(!buffer->ptr && !buffer->avail );
	buffer->ptr = (char *)g_malloc(len);
	buffer->avail = len;
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
		if(buffer->avail > (128*1024))
		{
			logprintf("%s:%d WARNING - realloc. buf %p avail %d required %d", __FUNCTION__, __LINE__, buffer, (int)buffer->avail, (int)required);
		}
		buffer->avail = required * 2; // grow generously to minimize realloc overhead
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
 * @brief Append nul character to buffer
 * @param buffer buffer in which nul to be append
 */
void aamp_AppendNulTerminator(struct GrowableBuffer *buffer)
{
	char zeros[2] = { 0, 0 }; // append two bytes, to distinguish between internal inserted 0x00's and a final 0x00 0x00
	aamp_AppendBytes(buffer, zeros, 2);
}
