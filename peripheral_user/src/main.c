/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>

#include <gpio.h>

#include <stdio.h>
#include <stdlib.h>

#include "services/ble_user.h"


#define OUTPUT_BUF_SIZE sizeof( unsigned int ) * 3
static u8_t user_vals[ OUTPUT_BUF_SIZE ];


static struct bt_conn* p_conn = NULL;
static struct bt_gatt_exchange_params exchange_params;
/** Start security procedure from Peripheral to NUS Central on nRF5 or SmartPhone */
/** BT_SECURITY_LOW(1)     No encryption and no authentication. */
/** BT_SECURITY_MEDIUM(2)  Encryption and no authentication (no MITM). */
/** BT_SECURITY_HIGH(3)    Encryption and authentication (MITM). */
/** BT_SECURITY_FIPS(4)    Authenticated Secure Connections !!CURRENTLY NOT USABLE!!*/
#define BT_SECURITY     BT_SECURITY_LOW
bt_security_t           g_level = BT_SECURITY_NONE;

//BLE Advertise
static volatile u8_t mfg_data[] = { 0x00, 0x00, 0x79, 0x23 };

static const struct bt_data ad[] = 
{
    BT_DATA_BYTES( BT_DATA_FLAGS, ( BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR ) ),
    BT_DATA( BT_DATA_MANUFACTURER_DATA, mfg_data, 4 ),
    
    BT_DATA_BYTES( BT_DATA_UUID128_ALL,
                   0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
                   0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11),
};

static void exchange_func( struct bt_conn* conn, u8_t err,
                           struct bt_gatt_exchange_params* params )
{
    struct bt_conn_info info = { 0 };
    
    printk( "MTU exchange %s\n", err == 0 ? "successful" : "failed" );
    
    err = bt_conn_get_info( conn, &info );
    if (info.role == BT_CONN_ROLE_MASTER) 
    {
        
    }
}

static void connected( struct bt_conn* conn, u8_t err )
{
    if( err ) 
    {
        printk( "Connection failed (err %u)\n", err );
    } 

    p_conn = bt_conn_ref( conn );
    printk( "Connected\n" );

    int ret = bt_conn_security( p_conn, BT_SECURITY );
    if( ret ) 
    {
        printk( "Failed to set security (err %d)\n", ret );
    }
}

static void disconnected( struct bt_conn* conn, u8_t reason )
{
    printk( "Disconnected (reason %u)\n", reason);

    if( p_conn ) 
    {
        bt_conn_unref( p_conn );
        p_conn = NULL;
    }
}

#if defined( CONFIG_BT_SMP )
static void identity_resolved( struct bt_conn* conn, const bt_addr_le_t* rpa, const bt_addr_le_t* identity )
{
	char addr_identity[ BT_ADDR_LE_STR_LEN ];
	char addr_rpa[ BT_ADDR_LE_STR_LEN ];

	bt_addr_le_to_str( identity, addr_identity, sizeof( addr_identity ) );
	bt_addr_le_to_str( rpa, addr_rpa, sizeof( addr_rpa ) );

	printk( "Identity resolved %s -> %s\n", addr_rpa, addr_identity );
}

static void security_changed( struct bt_conn* conn, bt_security_t level )
{
	char addr[ BT_ADDR_LE_STR_LEN ];

	bt_addr_le_to_str( bt_conn_get_dst( conn ), addr, sizeof( addr ) );

	printk( "Security changed: %s level %u\n", addr, level );
	g_level = level;
}
#endif /* defined( CONFIG_BT_SMP ) */

static struct bt_conn_cb conn_callbacks = {
	.connected          = connected,
	.disconnected       = disconnected,
	.le_param_req       = NULL,
	.le_param_updated   = NULL,
#if defined(CONFIG_BT_SMP)
	.identity_resolved  = identity_resolved,
	.security_changed   = security_changed
#endif /* defined(CONFIG_BT_SMP) */
};

static void bt_ready( int err )
{
    if( err ) 
    {
        printk( "Bluetooth init failed (err %d)\n", err );
        return;
    }
    
    printk( "Bluetooth initialized\n" );
    
    if( IS_ENABLED( CONFIG_SETTINGS ) ) 
    {
        settings_load();
    }
    
    exchange_params.func = exchange_func;
    err = bt_gatt_exchange_mtu( NULL, &exchange_params );
    
    err = bt_le_adv_start( BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE( ad ), NULL, 0 );
    if( err ) 
    {
        printk( "Advertising failed to start (err %d)\n", err );
        return;
    }
    
    printk( "Advertising successfully started\n" );
}

// User data
void update_user_data()
{
    static unsigned int counter = 0.0f;
    
    counter = counter + 1;
    printf( "counter: %d\n", counter );
    
    memset( user_vals, 0, sizeof( user_vals ) );
    
    ( ( unsigned int* )user_vals )[ 0 ] = counter;
    ( ( unsigned int* )user_vals )[ 1 ] = counter + 1;
    ( ( unsigned int* )user_vals )[ 2 ] = counter + 2;
}


void main( void )
{
    /* Set LED pin as output */
    struct device* port0 = device_get_binding("GPIO_0");
    gpio_pin_configure(port0, 17, GPIO_DIR_OUT);
    
    // flash  LED
    gpio_pin_write(port0, 17, 0);
    k_sleep(500);
    gpio_pin_write(port0, 17, 1);
    k_sleep(500);
    
    // set up BLE
    int err;
    err = bt_enable( bt_ready );
    if (err) 
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    
    bt_conn_cb_register( &conn_callbacks );
    
    while (1) 
    {
        if( p_conn != NULL )
        {
            if( user_is_notify() )
            {
                update_user_data();
                user_notify( p_conn, user_vals, OUTPUT_BUF_SIZE );
            }
        }
        k_sleep(100);
    }
    
}
