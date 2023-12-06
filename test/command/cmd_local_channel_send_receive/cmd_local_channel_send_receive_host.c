/*
 * Copyright (c) 2023, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "test_database.h"
#include "val_host_rmi.h"

#define CLIENT_REALM 0
#define SERVER_REALM 1

static void set_rec_entry_gprs(val_host_rec_entry_ts *rec_entry, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    rec_entry->gprs[1] = arg1;
    rec_entry->gprs[2] = arg2;
    rec_entry->gprs[3] = arg3;
}

void cmd_local_channel_send_receive_host(void)
{
    val_host_realm_ts client_realm, server_realm, realms[2];
    val_host_rmifeatureregister0_ts features_0;
    val_host_rec_entry_ts *rec_entries[2] = {};
    val_host_rec_exit_ts *rec_exit;
    uint64_t lc_ipa, lc_pa, lc_size = PAGE_SIZE, data_size;
    uint64_t ret;

    val_memset(&client_realm, 0, sizeof(client_realm));
    val_memset(&features_0, 0, sizeof(features_0));

    features_0.s2sz = 40;
    val_memcpy(&client_realm.realm_feat_0, &features_0, sizeof(features_0));

    client_realm.hash_algo = RMI_HASH_SHA_256;
    client_realm.s2_starting_level = 0;
    client_realm.num_s2_sl_rtts = 1;
    client_realm.vmid = 0;
    client_realm.rec_count = 1;

    server_realm = client_realm;
    server_realm.vmid = 1;

    realms[0] = client_realm;
    realms[1] = server_realm;

    lc_ipa = (1UL << (features_0.s2sz - 1)) - (PAGE_SIZE * 2);
    lc_pa = (uint64_t)val_host_mem_alloc(PAGE_SIZE, lc_size);
    if (!lc_pa)
    {
        LOG(ERROR, "\tval_host_mem_alloc failed\n", 0, 0);
        val_set_status(RESULT_FAIL(VAL_ERROR_POINT(3)));
        goto destroy_realm;
    }

    LOG(TEST, "\t[HOST] Set up local channel:\n", 0, 0);
    LOG(TEST, "\t[HOST] - PA: 0x%x\n", lc_pa, 0);
    LOG(TEST, "\t[HOST] - IPA: 0x%x\n", lc_ipa, 0);
    LOG(TEST, "\t[HOST] - Size: 0x%x\n", lc_size, 0);

    for (int i = 0; i < 2; i++)
    {
        /* Populate realm with one REC*/
        if (val_host_realm_setup(&realms[i], false))
        {
            LOG(ERROR, "\trealms[%d]: Realm setup failed\n", (long unsigned int)i, 0);
            val_set_status(RESULT_FAIL(VAL_ERROR_POINT(1)));
            goto destroy_realm;
        }

        if (val_host_ripas_init(&realms[i], lc_ipa, VAL_RTT_MAX_LEVEL, PAGE_SIZE))
        {
            LOG(ERROR, "\trealms[%d]: val_host_ripas_init failed\n", (long unsigned int)i, 0);
            val_set_status(RESULT_FAIL(VAL_ERROR_POINT(3)));
            goto destroy_realm;
        }

        rec_entries[i] = &(((val_host_rec_run_ts *)realms[i].run[0])->entry);
    }

    LOG(TEST, "\t[HOST] Call rmi_local_channel_setup()\n", 0, 0);
    val_host_rmi_local_channel_setup(realms[0].rd, realms[1].rd, lc_pa, lc_ipa, PAGE_SIZE);

    LOG(TEST, "\t[HOST] Run realms..\n\n", 0, 0);
    for (int realm_id = 1; realm_id >= 0; realm_id--)
    {
        /* Activate realms */
        if (val_host_realm_activate(&realms[realm_id]))
        {
            LOG(ERROR, "\trealms[%d]: Realm activate failed\n", (long unsigned int)realm_id, 0);
            val_set_status(RESULT_FAIL(VAL_ERROR_POINT(5)));
            goto destroy_realm;
        }

        /* Enter REC[0]  */
        ret = val_host_rmi_rec_enter(realms[realm_id].rec[0], realms[realm_id].run[0]);
        if (ret)
        {
            LOG(ERROR, "\tRec enter failed, ret=%x\n", ret, 0);
            val_set_status(RESULT_FAIL(VAL_ERROR_POINT(2)));
            goto destroy_realm;
        }
        else if (val_host_check_realm_exit_host_call((val_host_rec_run_ts *)realms[realm_id].run[0]))
        {
            LOG(ERROR, "\tREC_EXIT:  Rec Exit get_local_channel_info Pending\n", 0, 0);
            val_set_status(RESULT_FAIL(VAL_ERROR_POINT(3)));
            goto destroy_realm;
        }

        set_rec_entry_gprs(rec_entries[realm_id], lc_ipa, PAGE_SIZE, (uint64_t)realm_id);

        /* Enter REC[0]  */
        ret = val_host_rmi_rec_enter(realms[realm_id].rec[0], realms[realm_id].run[0]);
        if (ret)
        {
            LOG(ERROR, "\tRec enter failed, ret=%x\n", ret, 0);
            val_set_status(RESULT_FAIL(VAL_ERROR_POINT(2)));
            goto destroy_realm;
        }
    }

    for (int realm_id = 0; realm_id < 2; realm_id++)
    {
        switch (realm_id)
        {
        case CLIENT_REALM:

            rec_exit = &(((val_host_rec_run_ts *)realms[CLIENT_REALM].run[0])->exit);
            if (rec_exit->exit_reason != RMI_EXIT_LOCAL_CHANNEL_SEND)
            {
                LOG(ERROR, "\tExit_reason mismatch: %x\n", rec_exit->exit_reason, 0);
                goto destroy_realm;
            }
            lc_ipa = rec_exit->gprs[0];
            data_size = rec_exit->gprs[1];

            set_rec_entry_gprs(rec_entries[SERVER_REALM], lc_ipa, data_size, 0);
            break;
        case SERVER_REALM:
            if (val_host_check_realm_exit_host_call((val_host_rec_run_ts *)realms[realm_id].run[0]))
            {
                LOG(ERROR, "\tREC_EXIT:  Rec Exit local_channel_receive Pending\n", 0, 0);
                val_set_status(RESULT_FAIL(VAL_ERROR_POINT(3)));
                goto destroy_realm;
            }

            /* Enter REC[0]  */
            ret = val_host_rmi_rec_enter(realms[realm_id].rec[0], realms[realm_id].run[0]);
            if (ret)
            {
                LOG(ERROR, "\tRec enter failed, ret=%x\n", ret, 0);
                val_set_status(RESULT_FAIL(VAL_ERROR_POINT(2)));
                goto destroy_realm;
            }
            break;
        default:
            break;
        }
    }

    val_set_status(RESULT_PASS(VAL_SUCCESS));

/* Free test resources */
destroy_realm:
    return;
}
