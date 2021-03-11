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

/**
 * @file ThunderAccess.h
 * @brief shim for dispatching UVE HDMI input playback
 */

#ifndef THUNDERACCESS_H_
#define THUNDERACCESS_H_

#include <Module.h>
#include <core/core.h>
#include <websocket/websocket.h>

using namespace std;
using namespace WPEFramework;

#define THUNDER_RPC_TIMEOUT 5000

/**
 * @class ThunderAccessAAMP
 * @brief Support Thunder Plugin Access from AAMP
 */
class ThunderAccessAAMP
{
public:
    ThunderAccessAAMP(std::string callsign);
    ~ThunderAccessAAMP();
    // Copy constructor and Copy assignment disabled
    ThunderAccessAAMP(const ThunderAccessAAMP&) = delete;
    ThunderAccessAAMP& operator=(const ThunderAccessAAMP&) = delete;

    bool ActivatePlugin();
    bool InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime = THUNDER_RPC_TIMEOUT);
    bool SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
    bool UnSubscribeEvent (string eventName);

private:
    /*The Remote object connected to specific Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *remoteObject;
    /*The Remote object connected to controller Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *controllerObject;
    std::string pluginCallsign;
};
#endif // THUNDERACCESS_H_
