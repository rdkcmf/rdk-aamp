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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampJsonObject.h"
#include "subtec/subtecparser/TextStyleAttributes.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::An;
using ::testing::DoAll;

AampConfig *gpGlobalConfig = NULL;
AampLogManager *mLogObj = NULL;

class GetAttributesFontSizeTests : public ::testing::Test
{
protected:

    std::unique_ptr<TextStyleAttributes> mAttributes;

    void SetUp() override
    {
        mAttributes = std::unique_ptr<TextStyleAttributes>(new TextStyleAttributes(mLogObj));

        g_mockAampJsonObject = std::make_shared<StrictMock<MockAampJsonObject>>();
    }

    void TearDown() override
    {
        mAttributes = nullptr;

        g_mockAampJsonObject = nullptr;
    }
};

ACTION(ThrowJsonException)
{
    throw AampJsonParseException();
}

/*
    Test the getAttributes function supplying it with empty Json string
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetAttributesFontSizeTests, EmptyJsonOptionsString)
{
    std::string options{};
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_EQ(-1, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes function when AampJsonObject throws exception
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetAttributesFontSizeTests, JsonExceptionThrown)
{
    std::string options = "{\"fontSize\":\"32.4px\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>())).WillOnce(ThrowJsonException());
    EXPECT_EQ(-1, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes function when AampJsonObject unsuccessfully retrieves value
    A wrong key in the Json object (as set in options) is used to test the function.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetAttributesFontSizeTests, JsonValueNotReturned)
{
    std::string options = "{\"fontSize\":\"32.4px\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetAttributesFontSizeTests, RightKeyInvalidValueJsonOptionsString)
{
    std::string penSizeValue = "32.4px";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with font size small, penSizevalue expressed in lower case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueSmallLowerCase)
{
    std::string penSizeValue = "small";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_SMALL);
}

/*
    Test the getAttributes with font size small, penSizevalue expressed in Upper case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueSmallUpperCase)
{
    std::string penSizeValue = "SMALL";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_SMALL);
}

/*
    Test the getAttributes with font size Medium, penSizevalue expressed in lower case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueMediumLowerCase)
{
    std::string penSizeValue = "medium";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_STANDARD);
}

/*
    Test the getAttributes with font size Medium, penSizevalue expressed in Upper case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
    Two test cases are sufficient to prove that Upper case Json values are handled appropriately
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueMediumUpperCase)
{
    std::string penSizeValue = "MEDIUM";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_STANDARD);
}

/*
    Test the getAttributes with font size Standard
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueStandard)
{
    std::string penSizeValue = "standard";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_STANDARD);
}

/*
    Test the getAttributes with font size large
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueLarge)
{
    std::string penSizeValue = "large";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_LARGE);
}

/*
    Test the getAttributes with font size FONT_SIZE_EXTRALARGE
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueExtralarge)
{
    std::string penSizeValue = "extra_large";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_EXTRALARGE);
}

/*
    Test the getAttributes with font size auto i.e. embedded
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetAttributesFontSizeTests, ExpectedJsonOptionsStringValueAuto)
{
    std::string penSizeValue = "auto";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockAampJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_EMBEDDED);
}
