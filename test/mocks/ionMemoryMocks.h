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

#ifndef AAMP_ION_MOCKS_H
#define AAMP_ION_MOCKS_H

#include <stddef.h>
#include <sys/types.h>
#include <gmock/gmock.h>

#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }

#include <ion/ion.h>

class AampMockIon
{
	public:
		MOCK_METHOD(int, ion_open, ());
		MOCK_METHOD(int, ion_close, (int fd));
		MOCK_METHOD(int, ion_alloc, (int fd, size_t len, size_t align, unsigned int heap_mask, unsigned int flags, ion_user_handle_t *handle));
		MOCK_METHOD(int, ion_free, (int fd, ion_user_handle_t handle));
		MOCK_METHOD(int, ion_share, (int fd, ion_user_handle_t handle, int *share_fd));
		MOCK_METHOD(int, ion_phys, (int fd, ion_user_handle_t handle, unsigned long *addr, unsigned int *size));
};

extern AampMockIon *g_mockIon;

#endif /* AAMP_ION_MOCKS_H */
