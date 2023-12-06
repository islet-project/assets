/*
 * Copyright (c) 2023, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "test_database.h"
#include "val_realm_rsi.h"
#include "val_realm_framework.h"

#define INVALID_RIPAS_VALUE 0x2

#define CLIENT_REALM 0
#define SERVER_REALM 1
#define MAX_REALM 2

#define MAX_DATA 4

void cmd_local_channel_send_receive_realm(void)
{
    val_realm_rsi_host_call_t *gv_realm_host_call;
    val_pgt_descriptor_ts pgt_desc;
    val_memory_region_descriptor_ts mem_desc;
    uint64_t lc_size, lc_ipa, data_size, realm_id;
    uint64_t data[MAX_REALM][MAX_DATA] = {{0xC0FFEE, 0xBEEF, 0xF00D, 0xBA0BAB}, {}};

    gv_realm_host_call = val_realm_rsi_host_call_get_lc_info(VAL_SWITCH_TO_HOST);
    lc_ipa = gv_realm_host_call->gprs[1];
    lc_size = gv_realm_host_call->gprs[2];
    realm_id = gv_realm_host_call->gprs[3];

    if (realm_id == CLIENT_REALM)
    {
        LOG(TEST, "\t[CLIENT REALM] Client Realm is running..\n", 0, 0);
        LOG(TEST, "\t[CLIENT REALM] Get local channel info:\n", 0, 0);
        LOG(TEST, "\t[CLIENT REALM] - IPA: 0x%x\n", lc_ipa, 0);
        LOG(TEST, "\t[CLIENT REALM] - Size: 0x%x\n", lc_size, 0);
    }
    else
    {
        LOG(TEST, "\t[SERVER REALM] Server Realm is running..\n", 0, 0);
        LOG(TEST, "\t[SERVER REALM] Get local channel info:\n", 0, 0);
        LOG(TEST, "\t[SERVER REALM] - IPA: 0x%x\n", lc_ipa, 0);
        LOG(TEST, "\t[SERVER REALM] - Size: 0x%x\n", lc_size, 0);
    }

    pgt_desc.ttbr = tt_l0_base;
    pgt_desc.stage = PGT_STAGE1;
    pgt_desc.ias = PGT_IAS;
    pgt_desc.oas = PAGT_OAS;

    mem_desc.virtual_address = lc_ipa;
    mem_desc.physical_address = lc_ipa;
    mem_desc.length = lc_size;
    mem_desc.attributes = ATTR_RW_DATA | ATTR_NS;
    if (val_pgt_create(pgt_desc, &mem_desc))
    {
        LOG(ERROR, "\tVA to PA mapping failed. size: %x\n", lc_size, 0);
        val_set_status(RESULT_FAIL(VAL_ERROR_POINT(1)));
        goto exit;
    }

    if (realm_id == CLIENT_REALM)
    {
        LOG(TEST, "\t[CLIENT REALM] Stage 1 Mapping for the local channel is done\n", 0, 0);
    }
    else
    {
        LOG(TEST, "\t[SERVER REALM] Stage 1 Mapping for the local channel is done\n", 0, 0);
    }

    switch (realm_id)
    {
    case CLIENT_REALM:
        // data[realm_id][0] = 0xC0FFEE;

        val_memcpy((void *)lc_ipa, data[realm_id], MAX_DATA * sizeof(uint64_t));

        LOG(TEST, "\t[CLIENT REALM] Write the following data to the local channel: \n", 0, 0);
        for (uint64_t i = 0; i < MAX_DATA; i++)
        {
            LOG(TEST, "\t[CLIENT REALM] - 0x%lx \n", data[realm_id][i], 0);
        }

        LOG(TEST, "\t[CLIENT REALM] Call rsi_local_channel_send()\n\n", 0, 0);
        val_realm_rsi_local_channel_send(lc_ipa, lc_size, MAX_DATA * sizeof(uint64_t));
        break;
    case SERVER_REALM:
        LOG(TEST, "\t[SERVER REALM] Waiting for a client's request..\n\n", (uint64_t)data[realm_id][0], 0);
        gv_realm_host_call = val_realm_rsi_host_call_local_channel_receive(VAL_SWITCH_TO_HOST);
        lc_ipa = gv_realm_host_call->gprs[1];
        data_size = gv_realm_host_call->gprs[2];

        val_memcpy(data[realm_id], (const void *)lc_ipa, data_size);

        LOG(TEST, "\t[SERVER REALM] Get the following data in the local channel: \n", 0, 0);
        for (uint64_t i = 0; i < data_size / sizeof(uint64_t); i++)
        {
            LOG(TEST, "\t[SERVER REALM] - 0x%lx \n", data[realm_id][i], 0);
        }
        break;
    default:
        break;
    }

exit:
    val_realm_return_to_host();
}
