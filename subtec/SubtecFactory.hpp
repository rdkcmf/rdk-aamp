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

#include <string>

#include "WebvttSubtecDevParser.hpp"
#include "WebVttSubtecParser.hpp"
#include "TtmlSubtecParser.hpp"
#include "subtitleParser.h" //required for gpGlobalConfig also
#include "SubtecPacket.hpp" // for make_unique

class SubtecFactory
{
public:
    static std::unique_ptr<SubtitleParser> createSubtitleParser(AampLogManager *mLogObj, PrivateInstanceAAMP *aamp, std::string mimeType)
    {
        SubtitleMimeType type = eSUB_TYPE_UNKNOWN;

        AAMPLOG_INFO("createSubtitleParser: mimeType %s\n", mimeType.c_str());

        if (!mimeType.compare("text/vtt"))
            type = eSUB_TYPE_WEBVTT;
        else if (!mimeType.compare("application/ttml+xml") ||
                !mimeType.compare("application/mp4"))
            type = eSUB_TYPE_TTML;

        return createSubtitleParser(mLogObj, aamp, type);
    }

    static std::unique_ptr<SubtitleParser> createSubtitleParser(AampLogManager *mLogObj, PrivateInstanceAAMP *aamp, SubtitleMimeType mimeType)
    {
        AAMPLOG_INFO("createSubtitleParser: mimeType: %d\n", mimeType);
        std::unique_ptr<SubtitleParser> empty;
        
        try {
            switch (mimeType)
            {
                case eSUB_TYPE_WEBVTT:
                    // If JavaScript cue listeners have been registered use WebVTTParser,
                    // otherwise use subtec
                    if (!aamp->WebVTTCueListenersRegistered())
			if (ISCONFIGSET(eAAMPConfig_WebVTTNative))
                            return make_unique<WebVTTSubtecParser>(mLogObj, aamp, mimeType);
                        else
                            return make_unique<WebVTTSubtecDevParser>(mLogObj, aamp, mimeType);
                    else
                        return make_unique<WebVTTParser>(mLogObj, aamp, mimeType);
                case eSUB_TYPE_TTML:
                    return make_unique<TtmlSubtecParser>(mLogObj, aamp, mimeType);
                default:
                    AAMPLOG_WARN("Unknown subtitle parser type %d, returning empty", mimeType);
                    break;
            }
        } catch (const std::runtime_error &e) {
            AAMPLOG_WARN("%s", e.what());
            AAMPLOG_WARN(" Failed on SubtitleParser construction - returning empty");
        }

        return empty;
    }
};
