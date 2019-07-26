/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_user.h"

/* Custom Service Variables */
static struct bt_uuid_128 user_service_uuid = BT_UUID_INIT_128(
                                                              0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
                                                              0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11);


static const struct bt_uuid_128 user_char_uuid = BT_UUID_INIT_128(
                                                                 0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
                                                                 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x12);

static struct bt_gatt_ccc_cfg user_ccc_cfg[BT_GATT_CCC_MAX];

static volatile u8_t notifyEnable;

#define BT_BUF sizeof( unsigned int ) * 3
static u8_t user_data[ BT_BUF ];

//User Descriptor
#define USER_CUD          "User"


static void user_ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value)
{
    notifyEnable = (value == BT_GATT_CCC_NOTIFY) ? true : false;
}

BT_GATT_SERVICE_DEFINE( user_svc,
    /* Vendor Primary Service Declaration */
    BT_GATT_PRIMARY_SERVICE( &user_service_uuid ),
    
    BT_GATT_CHARACTERISTIC( &user_char_uuid.uuid,
                            BT_GATT_CHRC_NOTIFY,
                            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                            NULL, NULL, user_data ),

    BT_GATT_CUD(USER_CUD, BT_GATT_PERM_READ), //Add User Descriptor
    
    BT_GATT_CCC(user_ccc_cfg, user_ccc_cfg_changed),
);


static int user_init( struct device* dev )
{
    printk( "[%s]\n", __func__ );
    return 0;
}

bool user_is_notify(void)
{
    return notifyEnable;
}

void user_notify(struct bt_conn* conn, u8_t *p_vals, u16_t len)
{
    if( conn == NULL )
    {
        return;
    }
    
    if( !notifyEnable )
    {
        return;
    }
    
    if( len > BT_BUF )
    {
        return;
    }
    
    memset( user_data, 0, sizeof( user_data ) );
    memcpy( user_data, p_vals, BT_BUF );
    
    bt_gatt_notify( conn, &user_svc.attrs[ 1 ], ( u8_t* )user_data, BT_BUF );
}

SYS_INIT( user_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY );
