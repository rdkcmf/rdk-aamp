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
#include <iostream>
#include <fstream>
#include "MockPlayerInstanceAAMP.h"
#include "AampcliSet.h"

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

class ExecuteTests : public ::testing::Test
{
protected:
    Set *mSet = nullptr;

    void SetUp() override 
    {
        mSet = new Set();

        g_mockPlayerInstanceAAMP = new MockPlayerInstanceAAMP();
    }
    
    void TearDown() override 
    {
        delete g_mockPlayerInstanceAAMP;
        g_mockPlayerInstanceAAMP = nullptr;

        delete mSet;
        mSet =nullptr;
    }
};

// Test calling "set ssStyle" with a valid default option
TEST_F(ExecuteTests, Set_CCStyle_NumberedOptions)
{
    char cmd1[] = "set ccStyle 1";
    char cmd2[] = "set ccStyle 2";
    char cmd3[] = "set ccStyle 3";
    
    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(CC_OPTION_1)).Times(1);
    mSet->execute(cmd1, g_mockPlayerInstanceAAMP);

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(CC_OPTION_2)).Times(1);
    mSet->execute(cmd2, g_mockPlayerInstanceAAMP);

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(CC_OPTION_3)).Times(1);
    mSet->execute(cmd3, g_mockPlayerInstanceAAMP);
}

// Test calling "set 45" (ccStyle) with an valid default option
TEST_F(ExecuteTests, Set_45_NumberedOptions)
{
    char cmd1[] = "set 45 1";
    
    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(CC_OPTION_1)).Times(1);
    mSet->execute(cmd1, g_mockPlayerInstanceAAMP);
}

// Test calling "set ssStyle" with no parameter
TEST_F(ExecuteTests, Set_CCStyle_NoParameter)
{
    char cmd[] = "set ccStyle ";

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(_)).Times(0);
    mSet->execute(cmd, g_mockPlayerInstanceAAMP);
}

// Test calling "set ssStyle" with an invalid default option
TEST_F(ExecuteTests, Set_CCStyle_InvalidNumberedOption)
{
    char cmd0[] = "set ccStyle 0";
    char cmd4[] = "set ccStyle 4";

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(_)).Times(0);
    mSet->execute(cmd0, g_mockPlayerInstanceAAMP);

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(_)).Times(0);
    mSet->execute(cmd4, g_mockPlayerInstanceAAMP);
}

// Test calling "set ssStyle" with a valid json file
// Note: This test Creates a json file in the build output directory, and then removes it
TEST_F(ExecuteTests, Set_CCStyle_File)
{
    const char json_file[] = "test_json_file";
    char cmd[] = "set ccStyle test_json_file";
    const char json[] = "{ \"penSize\":\"small\" }";

    std::ifstream ifs(json_file, std::ifstream::in);
    ASSERT_FALSE(ifs.good());

    // Creates a json file in the build output directory
    std::ofstream of(json_file, std::ofstream::out);
    ASSERT_TRUE(of.is_open());

    of << json << std::endl;
    of.flush();
    of.close();

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(json)).Times(1);
    mSet->execute(cmd, g_mockPlayerInstanceAAMP);

    // Remove json file
    ASSERT_FALSE(std::remove(json_file));
}

// Test calling "set ssStyle" with a valid json file which starts with an option number
// Make sure file is opened, and does not assume an option number
// Note: This test Creates a json file in the build output directory, and then removes it
TEST_F(ExecuteTests, Set_CCStyle_FileStartingWithNumber)
{
    const char json_file[] = "1test_json_file";
    char cmd[] = "set ccStyle 1test_json_file";
    const char json[] = "{ \"penSize\":\"small\" }";

    std::ifstream ifs(json_file, std::ifstream::in);
    ASSERT_FALSE(ifs.good());

    // Creates a json file in the build output directory
    std::ofstream of(json_file, std::ofstream::out);
    ASSERT_TRUE(of.is_open());

    of << json << std::endl;
    of.flush();
    of.close();

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(json)).Times(1);
    mSet->execute(cmd, g_mockPlayerInstanceAAMP);

    // Remove json file
    ASSERT_FALSE(std::remove(json_file));
}

// Test calling "set ssStyle" with a file that doesn't exist
TEST_F(ExecuteTests, Set_CCStyle_MissingFile)
{
    char cmd[] = "set ccStyle MissingFile";

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(_)).Times(0);
    mSet->execute(cmd, g_mockPlayerInstanceAAMP);
}

// Test calling "set ssStyle" with a file that exceeds the file path buffer
TEST_F(ExecuteTests, Set_CCStyle_FilenameExceedsMaxLength)
{
    char cmd [255] = "set ccStyle ";
    int i;

    // Fill array with characters and null terminate
    for (i = strlen(cmd); i < sizeof(cmd) - 1; i++)
    {
        cmd[i] = 'A' + (i % 26);
    }
    cmd[i] = 0;

    EXPECT_CALL(*g_mockPlayerInstanceAAMP, SetTextStyle(_)).Times(0);
    mSet->execute(cmd, g_mockPlayerInstanceAAMP);
}

