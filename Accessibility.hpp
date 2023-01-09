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

#ifndef __ACCESSIBILITY__
#define __ACCESSIBILITY__

#include <string>

/**
 * @class Accessibility
 * @brief Data type to store Accessibility Node data
 */
class Accessibility
{
	std::string strSchemeId;
	int intValue;
	std::string strValue;
	std::string valueType;
	
	bool isNumber(const std::string& str)
	{
		std::string::const_iterator it = str.begin();
		while (it != str.end() && std::isdigit(*it)) ++it;
		return !str.empty() && it == str.end();
	};

  public:
	Accessibility(std::string schemId, std::string val): strSchemeId(schemId), intValue(-1), strValue(""), valueType("")
	{
		if (isNumber(val))
		{
			valueType = "int_value";
			intValue = std::stoi(val);
			strValue = "";
		}
		else
		{
			valueType = "string_value";
			intValue = -1;
			strValue = val;
		}

	};

	Accessibility():strSchemeId(""), intValue(-1), strValue(""), valueType("") {};

	void setAccessibilityData(std::string schemId, std::string val)
	{
		strSchemeId = schemId;
		if (isNumber(val))
		{
			valueType = "int_value";
			intValue = std::stoi(val);
			strValue = "";
		}
		else
		{
			valueType = "string_value";
			intValue = -1;
			strValue = val;
		}
	};

	void setAccessibilityData(std::string schemId, int val)
	{
		strSchemeId = schemId;
		valueType = "int_value";
		intValue = val;
		strValue = "";
	};

	std::string& getTypeName() {return valueType;};
	std::string& getSchemeId() {return strSchemeId;};
	int getIntValue() {return intValue;};
	std::string& getStrValue() {return strValue;};
	void clear()
	{
		strSchemeId = "";
		intValue = -1;
		strValue = "";
		valueType = "";
	};

	bool operator == (const Accessibility& track) const
	{
		return ((strSchemeId == track.strSchemeId) &&
			(valueType == "int_value"?(intValue == track.intValue):(strValue == track.strValue)));
	};

	bool operator != (const Accessibility& track) const
	{
		return ((strSchemeId != track.strSchemeId) ||
			(valueType == "int_value"?(intValue != track.intValue):(strValue != track.strValue)));
	};
	
	Accessibility& operator = (const Accessibility& track)
	{
		strSchemeId = track.strSchemeId;
		intValue = track.intValue;
		strValue = track.strValue;
		valueType = track.valueType;
		
		return *this;
	};

	std::string print()
	{
		char strData [228];
		std::string retVal = "";
		if (!strSchemeId.empty())
		{
			std::snprintf(strData, sizeof(strData), "{ scheme:%s, %s:", strSchemeId.c_str(), valueType.c_str());
			retVal += strData;
			if (valueType == "int_value")
			{
				std::snprintf(strData, sizeof(strData), "%d }", intValue);
			}else
			{
				std::snprintf(strData, sizeof(strData), "%s }", strValue.c_str());
			}
			retVal += strData;
		}
		else
		{
			retVal = "NULL";
		}
		return retVal;
	};
};

#endif // __ACCESSIBILITY__

