/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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

#include "ionMemoryMocks.h"

AampMockIon *g_mockIon;

int ion_open()
{
	return g_mockIon->ion_open();
}

int ion_close(int fd)
{
	return g_mockIon->ion_close(fd);
}

int ion_alloc(int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags, ion_user_handle_t *handle)
{
	return g_mockIon->ion_alloc(fd, len, align, heap_mask, flags, handle);
}

int ion_free(int fd, ion_user_handle_t handle)
{
	return g_mockIon->ion_free(fd, handle);
}

int ion_share(int fd, ion_user_handle_t handle, int *share_fd)
{
	return g_mockIon->ion_share(fd, handle, share_fd);
}

int ion_phys(int fd, ion_user_handle_t handle, unsigned long *addr, unsigned int *size)
{
	return g_mockIon->ion_phys(fd, handle, addr, size);
}
