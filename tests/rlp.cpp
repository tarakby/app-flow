/*******************************************************************************
*   (c) 2020 Zondax GmbH
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "gmock/gmock.h"
#include <iostream>
#include <hexutils.h>
#include "rlp.h"

using ::testing::TestWithParam;
using ::testing::Values;

struct RLPValueTestCase {
    const char *data;
    uint8_t expectedKind;
    uint64_t expectedLen;
    uint64_t expectedDataOffset;
    uint64_t expectedConsumed;
};

class RLPDecodeTest : public testing::TestWithParam<RLPValueTestCase> {
public:
    void SetUp() override {}

    void TearDown() override {}
};

INSTANTIATE_TEST_SUITE_P(
        InstantiationName,
        RLPDecodeTest,
        Values(
                RLPValueTestCase{"00", kind_byte, 1, 0, 1},
                RLPValueTestCase{"01", kind_byte, 1, 0, 1},
                RLPValueTestCase{"7F", kind_byte, 1, 0, 1},

                RLPValueTestCase{"80", kind_string, 0, 1, 1},
                RLPValueTestCase{"B7", kind_string, 55, 1, 56},
                RLPValueTestCase{"B90400", kind_string, 1024, 3, 1027},

                RLPValueTestCase{"C0", kind_list, 0, 1, 1},
                RLPValueTestCase{"C8", kind_list, 8, 1, 9},
                RLPValueTestCase{"F7", kind_list, 55, 1, 56},
                RLPValueTestCase{"F90400", kind_list, 1024, 3, 1027}
        )
);

TEST_P(RLPDecodeTest, decodeElement) {
    auto params = GetParam();

    uint8_t data[100];
    parseHexString(data, sizeof(data), params.data);

    parser_context_t ctx_in;
    parser_context_t ctx_out;

    ctx_in.buffer = data;
    ctx_in.offset = 0;
    ctx_in.bufferLen = sizeof(data);

    rlp_kind_e kind;
    uint16_t bytesConsumed;

    parser_error_t err = rlp_decode(&ctx_in, &ctx_out, &kind, &bytesConsumed);

    EXPECT_THAT(err, parser_ok);
    EXPECT_THAT(kind, testing::Eq(params.expectedKind));
    EXPECT_THAT(ctx_out.bufferLen, testing::Eq(params.expectedLen));
    EXPECT_THAT(ctx_out.buffer - ctx_in.buffer, testing::Eq(params.expectedDataOffset));
    EXPECT_THAT(bytesConsumed, testing::Eq(params.expectedConsumed));
}
