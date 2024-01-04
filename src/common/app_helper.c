/*******************************************************************************
 *   (c) 2018, 2019 Zondax GmbH
 *   (c) 2016 Ledger
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

#include "app_helper.h"

#include <string.h>
#include <os_io_seproxyhal.h>
#include <os.h>

#include "view.h"
#include "actions.h"
#include "tx.h"
#include "crypto.h"
#include "coin.h"
#include "zxmacros.h"
#include "hdpath.h"
#include "tx_metadata.h"

void extractHDPathAndCryptoOptions(uint32_t rx, uint32_t offset) {
    if ((rx - offset) < sizeof(hdPath.data) + sizeof(cryptoOptions)) {
        THROW(APDU_CODE_WRONG_LENGTH);
    }

    MEMCPY(hdPath.data, G_io_apdu_buffer + offset, sizeof(hdPath.data));

    const bool mainnet = hdPath.data[0] == HDPATH_0_DEFAULT && hdPath.data[1] == HDPATH_1_DEFAULT;

    const bool testnet = hdPath.data[0] == HDPATH_0_TESTNET && hdPath.data[1] == HDPATH_1_TESTNET;

    if (!mainnet && !testnet) {
        THROW(APDU_CODE_DATA_INVALID);
    }

    MEMCPY(&cryptoOptions, G_io_apdu_buffer + offset + sizeof(hdPath.data), sizeof(cryptoOptions));
}

process_chunk_response_t process_chunk(__Z_UNUSED volatile uint32_t *tx, uint32_t rx) {
    const uint8_t payloadType = G_io_apdu_buffer[OFFSET_PAYLOAD_TYPE];
    const uint8_t p2 = G_io_apdu_buffer[OFFSET_P2];

    if (p2 != 0 && payloadType != 0x02) {
        THROW(APDU_CODE_INVALIDP1P2);
    }

    if (rx < OFFSET_DATA) {
        THROW(APDU_CODE_WRONG_LENGTH);
    }

    uint32_t added;
    switch (payloadType) {
        case 0x00:
            tx_initialize();
            tx_reset();
            extractHDPathAndCryptoOptions(rx, OFFSET_DATA);
            initStoredTxMetadata();
            return PROCESS_CHUNK_NOT_FINISHED;
        case 0x01:
            added = tx_append(&(G_io_apdu_buffer[OFFSET_DATA]), rx - OFFSET_DATA);
            if (added != rx - OFFSET_DATA) {
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }
            return PROCESS_CHUNK_NOT_FINISHED;
        case 0x02:
            added = tx_append(&(G_io_apdu_buffer[OFFSET_DATA]), rx - OFFSET_DATA);
            if (added != rx - OFFSET_DATA) {
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }
            switch (p2) {
                case 0x01:
                    zemu_log("Transaction witout metadata.\n");
                    return PROCESS_CHUNK_FINISHED_NO_METADATA;
                case 0x02:
                    zemu_log("Transaction to match nft1 script.\n");
                    return PROCESS_CHUNK_FINISHED_NFT1;
                case 0x03:
                    zemu_log("Transaction to match nft2 script.\n");
                    return PROCESS_CHUNK_FINISHED_NFT2;
                default:
                    zemu_log("Incalid P2 transaction type.\n");
                    THROW(APDU_CODE_INVALIDP1P2);
            }
        case 0x03:
            if (storeTxMetadata(&(G_io_apdu_buffer[OFFSET_DATA]), rx - OFFSET_DATA) != PARSER_OK) {
                initStoredTxMetadata();  // delete merkle tree proof on error for redundant security
                THROW(APDU_CODE_DATA_INVALID);
            }
            return PROCESS_CHUNK_NOT_FINISHED;
        case 0x04:
            if (validateStoredTxMetadataMerkleTreeLevel(&(G_io_apdu_buffer[OFFSET_DATA]),
                                                        rx - OFFSET_DATA) != PARSER_OK) {
                initStoredTxMetadata();  // delete merkle tree proof on error for redundant security
                THROW(APDU_CODE_DATA_INVALID);
            }
            return PROCESS_CHUNK_NOT_FINISHED;
        case 0x05:
            if (validateStoredTxMetadataMerkleTreeLevel(&(G_io_apdu_buffer[OFFSET_DATA]),
                                                        rx - OFFSET_DATA) != PARSER_OK) {
                initStoredTxMetadata();  // delete merkle tree proof on error for redundant security
                THROW(APDU_CODE_DATA_INVALID);
            }
            return PROCESS_CHUNK_FINISHED_WITH_METADATA;
        case 0x10:
            zemu_log("Message to sign.\n");
            added = tx_append(&(G_io_apdu_buffer[OFFSET_DATA]), rx - OFFSET_DATA);
            if (added != rx - OFFSET_DATA) {
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }
            return PROCESS_CHUNK_FINISHED_MESSAGE;
    }

    THROW(APDU_CODE_INVALIDP1P2);
}
