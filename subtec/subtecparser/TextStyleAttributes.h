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
 * @file  TextStyleAttributes.h
 *
 * @brief This file provides class and other definition related to subtitle text attributes
 *
 */

#pragma once

#include <string.h>
#include <array>
#include "AampLogManager.h"
#include "AampConfig.h"
#include "SubtecAttribute.hpp"

class TextStyleAttributes
{

public:
    /**
     * @enum FontSize
     * @brief Available Fontsize
     */
    typedef enum FontSize {
        FONT_SIZE_EMBEDDED = -1,            /* Corresponds to Font size of auto */
        FONT_SIZE_SMALL,
        FONT_SIZE_STANDARD,
        FONT_SIZE_LARGE,
        FONT_SIZE_EXTRALARGE,
        FONT_SIZE_MAX
    } FontSize;
    /**
     * @enum AttribPosInArray
     * @brief Provides the indexing postion in array for attributes
     */
    typedef enum AttribPosInArray {
        FONT_SIZE_ARR_POSITION = 5,
    } AttribPosInArray;

    TextStyleAttributes(AampLogManager *logObj);

    /**
     * @fn getAttributes
     * @param[in] options - Json string containing the attributes
     * @param[out] attributesValues - Extracted Attribute values (for now they are font size and position)
     * @param[out] attributesMask - Mask corresponding to extracted attribute values
     * @return int - 0 for success, -1 for failure
     */
    int getAttributes(std::string options, attributesType &attributesValues, uint32_t &attributesMask);

private:
    /**
     * @struct Attributes
     * @brief  Attributes, so far fontSize only.
     */
    struct Attributes {
        FontSize fontSize;
    };

    /**
     * @fn getFontSize
     * @param[in] input - input font size value
     * @param[out] fontSizeOut - font size option for the input value
     * @return int - 0 for success, -1 for failure
     */
    int getFontSize(std::string input, FontSize *fontSizeOut);

    AampLogManager* mLogObj;
};

