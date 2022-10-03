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
*
* Fake implementations of APIs from Curl which is:
* Copyright (c) 1996 - 2022, Daniel Stenberg, daniel@haxx.se, and many contributors
* Licensed under the CURL License
*/

#include <curl/curl.h>

CURL *curl_easy_init(void)
{
    return nullptr;
}

CURLcode curl_easy_setopt(CURL *handle, CURLoption option, ...)
{
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *curl)
{
    return CURLE_OK;
}

void curl_easy_cleanup(CURL *curl)
{
}

CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...)
{
    return CURLE_OK;
}

struct curl_slist *curl_slist_append(struct curl_slist *,
                                     const char *)
{
    return nullptr;
}

void curl_slist_free_all(struct curl_slist *)
{
}

CURLSHcode curl_share_cleanup(CURLSH *)
{
    return CURLSHE_OK;
}
