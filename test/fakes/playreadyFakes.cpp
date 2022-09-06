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

#define FAKE_FUNCTION(x) \
void x(); \
void x() {}

#define FAKE_SYMBOL(x) \
extern void *x; \
void *x;

extern "C" {
FAKE_SYMBOL(g_dstrDrmPath)
FAKE_SYMBOL(g_dstrWMDRM_RIGHT_PLAYBACK)

FAKE_FUNCTION(DRM_B64_EncodeA)
FAKE_FUNCTION(Drm_Content_SetProperty)
FAKE_FUNCTION(DRM_DWordAdd)
FAKE_FUNCTION(DRM_DWordMult)
FAKE_FUNCTION(DRM_DWordSub)
FAKE_FUNCTION(Drm_Initialize)
FAKE_FUNCTION(Drm_LicenseAcq_GenerateChallenge)
FAKE_FUNCTION(Drm_LicenseAcq_ProcessResponse)
FAKE_FUNCTION(Drm_Reader_Bind)
FAKE_FUNCTION(Drm_Reader_Close)
FAKE_FUNCTION(Drm_Reader_Commit)
FAKE_FUNCTION(Drm_Reader_Decrypt)
FAKE_FUNCTION(Drm_Reader_InitDecrypt)
FAKE_FUNCTION(Drm_Reinitialize)
FAKE_FUNCTION(DRM_REVOCATION_IsRevocationSupported)
FAKE_FUNCTION(Drm_Revocation_SetBuffer)
FAKE_FUNCTION(DRMCRT_memcmp)
FAKE_FUNCTION(DRMCRT_memcpy)
FAKE_FUNCTION(DRMCRT_memset)
FAKE_FUNCTION(Oem_MemAlloc)
FAKE_FUNCTION(Oem_MemFree)
FAKE_FUNCTION(Oem_Random_GetBytes)
}
