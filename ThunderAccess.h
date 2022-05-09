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
    /**
     *   @brief  ThunderAccessAAMP constructor
     *   @note   Security tocken accusition, controller object creation
     */
    ThunderAccessAAMP(std::string callsign, AampLogManager *logObj=NULL);
	
    /**
     *   @brief  ThunderAccessAAMP destructor
     *   @note   clean up
     */
    ~ThunderAccessAAMP();
   
    /**
     *   @brief  ThunderAccessAAMP copy constructor disabled
     */
    ThunderAccessAAMP(const ThunderAccessAAMP&) = delete;

    /**
     *   @brief  ThunderAccessAAMP assignment disabled
     */
    ThunderAccessAAMP& operator=(const ThunderAccessAAMP&) = delete;

    /**
     *   @brief  ActivatePlugin
     *   @note   Plugin activation and Remote object creation
     *   @param  Plugin Callsign
     *   @retval true on success
     *   @retval false on failure
     */
    bool ActivatePlugin();
	
    /**
     *   @brief  invokeJSONRPC
     *   @note   Invoke JSONRPC call for the plugin
     *   @param  method,param,result,  method,waitTime reference to input param, result and waitTime (default = THUNDER_RPC_TIMEOUT)
     *   @retval true on success
     *   @retval false on failure
     */
    bool InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime = THUNDER_RPC_TIMEOUT);
	
    /**
     *   @brief  subscribeEvent
     *   @note   Subscribe event data for the specific plugin
     *   @param  eventName,functionHandler Event name, Event handler
     *   @retval true on success
     *   @retval false on failure
     */
    bool SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
	
    /**
     *   @brief  unSubscribeEvent 
     *   @note   unSubscribe event data for the specific plugin
     *   @param  eventName Event name
     *   @retval true on success
     *   @retval false on failure
     */
    bool UnSubscribeEvent (string eventName);

private:
    /**< The Remote object connected to specific Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *remoteObject;
    /**< The Remote object connected to controller Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *controllerObject;
    std::string pluginCallsign;
    AampLogManager *mLogObj;
};
#endif // THUNDERACCESS_H_
