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

#include "AampConfig.h"
#include "MockAampConfig.h"

MockAampConfig *g_mockAampConfig = nullptr;

template void AampConfig::SetConfigValue<long>(ConfigPriority owner, AAMPConfigSettings cfg , const long &value);
template void AampConfig::SetConfigValue<double>(ConfigPriority owner, AAMPConfigSettings cfg , const double &value);
template void AampConfig::SetConfigValue<int>(ConfigPriority owner, AAMPConfigSettings cfg , const int &value);
template void AampConfig::SetConfigValue<bool>(ConfigPriority owner, AAMPConfigSettings cfg , const bool &value);

AampConfig::AampConfig()
{
}

AampConfig& AampConfig::operator=(const AampConfig& rhs) 
{
    return *this;
}

void AampConfig::Initialize()
{
}

bool AampConfig::IsConfigSet(AAMPConfigSettings cfg)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->IsConfigSet(cfg);
    }
    else
    {
        return false;
    }
}

bool AampConfig::GetConfigValue(AAMPConfigSettings cfg , int &value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg, value);
    }
    else
    {
        return false;
    }
}

bool AampConfig::GetConfigValue(AAMPConfigSettings cfg, long &value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg, value);
    }
    else
    {
        return false;
    }
}

bool AampConfig::GetConfigValue(AAMPConfigSettings cfg, double &value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg, value);
    }
    else
    {
        return false;
    }
}

bool AampConfig::GetConfigValue(AAMPConfigSettings cfg, std::string &value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg, value);
    }
    else
    {
        return false;
    }
}

template<typename T>
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettings cfg ,const T &value)
{
}

template <>
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettings cfg , const std::string &value)
{
}

void AampConfig::ReadDeviceCapability()
{
}

bool AampConfig::ReadAampCfgJsonFile()
{
    return false;
}

bool AampConfig::ReadAampCfgTxtFile()
{
    return false;
}

void AampConfig::ReadOperatorConfiguration()
{
}

void AampConfig::ShowOperatorSetConfiguration()
{
}

void AampConfig::ShowDevCfgConfiguration()
{
}

void AampConfig::RestoreConfiguration(ConfigPriority owner, AampLogManager *mLogObj)
{
}

bool AampConfig::ProcessConfigJson(const cJSON *cfgdata, ConfigPriority owner )
{
    return false;
}

void AampConfig::DoCustomSetting(ConfigPriority owner)
{
}

ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettings cfg)
{
    return AAMP_DEFAULT_SETTING;
}

bool AampConfig::GetAampConfigJSONStr(std::string &str)
{
    return false;
}

std::string AampConfig::GetUserAgentString()
{
	return "";
}

const char * AampConfig::GetChannelOverride(const std::string manifestUrl)
{
    return nullptr;
}

const char * AampConfig::GetChannelLicenseOverride(const std::string manifestUrl)
{
    return nullptr;
}

bool AampConfig::CustomSearch( std::string url, int playerId , std::string appname)
{
    return false;
}
