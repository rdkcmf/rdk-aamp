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

/**
 * @file TextStyleAttributes.cpp
 *
 * @brief This file provides implementation of class methods related to subtitle text attributes
 *
 */

#include <assert.h>
#include <cctype>
#include <algorithm>
#include <string>
#include "TextStyleAttributes.h"
#include "AampJsonObject.h"                     // For JSON parsing


TextStyleAttributes::TextStyleAttributes(AampLogManager* logObj) : mLogObj(logObj)
{
}

/**
 * @brief Get font size value from input string
 *
 * @param[in] input - input font size value
 * @param[out] fontSizeOut - font size option for the input value
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getFontSize(std::string input, FontSize *fontSizeOut)
{
	int retVal = 0;

	if (!input.empty() && fontSizeOut)
	{
		transform(input.begin(), input.end(), input.begin(), ::tolower);	/* Makes sure that string is in lower case before comparison */

		if (input == "small")
		{
			*fontSizeOut = FONT_SIZE_SMALL;
		}
		else if ((input == "standard") || (input =="medium"))
		{
			*fontSizeOut = FONT_SIZE_STANDARD;
		}
		else if (input == "large")
		{
			*fontSizeOut = FONT_SIZE_LARGE;
		}
		else if (input == "extra_large")
		{
			*fontSizeOut = FONT_SIZE_EXTRALARGE;
		}
		else if (input == "auto")
		{
			*fontSizeOut = FONT_SIZE_EMBEDDED;
		}
		else
		{
			AAMPLOG_ERR("Unsupported font size type %s", input.c_str());
			retVal = -1;
		}
	}
	else
	{
		AAMPLOG_ERR("Input is NULL");
		retVal = -1;
	}
	return retVal;
}

/**
 * @brief Gets Attributes of the subtitle
 *
 * @param[in] options - Json string containing the attributes
 * @param[out] attributesValues - Extracted Attribute values (for now they are font size and position)
 * @param[out] attributesMask - Mask corresponding to extracted attribute values
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getAttributes(std::string options, attributesType &attributesValues, uint32_t &attributesMask)
{
	int retVal = 0;
	attributesMask = 0;

	AAMPLOG_WARN("TextStyleAttributes::getAttributes");

	if (!options.empty())
	{
		std::string optionValue;
		Attributes attribute;
		try
		{
			AampJsonObject inputOptions(options);

			if (inputOptions.get("penSize", optionValue))
			{
				if(!getFontSize(optionValue, &(attribute.fontSize)))
				{
					attributesMask |= (1 << FONT_SIZE_ARR_POSITION);
					attributesValues[FONT_SIZE_ARR_POSITION] = attribute.fontSize;
					AAMPLOG_INFO("The font size is %d", attributesValues[FONT_SIZE_ARR_POSITION]);
				}
				else
				{
					AAMPLOG_WARN("Can not parse penSize value of %s", optionValue.c_str());
				}
			}
		}
		catch(const AampJsonParseException& e)
		{
			AAMPLOG_ERR("TextStyleAttributes: AampJsonParseException - %s", e.what());
			return -1;
		}
	}
	else
	{
		retVal = -1;
		AAMPLOG_WARN("Empty input Json string");
	}
	return retVal;
}
