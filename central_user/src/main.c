/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>


/* Custom Service Variables */
static struct bt_uuid_128 user_service_uuid = BT_UUID_INIT_128(
                                                              0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
                                                              0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11);


static const struct bt_uuid_128 user_char_uuid = BT_UUID_INIT_128(
                                                              0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
                                                              0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x12);
#define BT_SECURITY     BT_SECURITY_NONE
static struct bt_conn* default_conn;

static struct bt_uuid_128 uuid = BT_UUID_INIT_128( 0 );
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static u8_t notify_func( struct bt_conn* conn,
			             struct bt_gatt_subscribe_params *params,
			             const void* data, 
                         u16_t length )
{
    int i;
	if( !data ) 
    {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	printk( "[NOTIFICATION] length: %u, data: ", length );

    for( i = 0; i < length / sizeof( unsigned int ); ++i )
	    printk( "%d ", ( ( unsigned* )data )[ i ] );
	printk( "\n" );

	return BT_GATT_ITER_CONTINUE;
}

static u8_t discover_func( struct bt_conn* conn,
			               const struct bt_gatt_attr* attr,
			               struct bt_gatt_discover_params* params )
{
	int err;

	if (!attr) {
		printk("Discover complete\n");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	//printk("[ATTRIBUTE] handle %u\n", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, (const struct bt_uuid *)&user_service_uuid)) {
		memcpy(&uuid, &user_char_uuid, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				(const struct bt_uuid *)&user_char_uuid )) {
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else {
		subscribe_params.notify = notify_func;
		subscribe_params.value = BT_GATT_CCC_NOTIFY;
		subscribe_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			printk("Subscribe failed (err %d)\n", err);
		} else {
			printk("[SUBSCRIBED]\n");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}
static void connected(struct bt_conn *conn, u8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);
		return;
	}

	printk("Connected: %s\n", addr);

	if (conn == default_conn) {
		memcpy(&uuid, &user_service_uuid, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = 0x0001;
		discover_params.end_handle = 0xffff;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			printk("Discover failed(err %d)\n", err);
			return;
		}
	}
}

static bool eir_found(struct bt_data *data, void *user_data)
{
    bt_addr_le_t *addr = user_data;

    //printk("[AD]: %u data_len %u\n", type, data_len);

    switch( data->type ) 
    {
        case BT_DATA_UUID128_SOME:
        case BT_DATA_UUID128_ALL:
            {
                if( data->data_len % 16 != 0 ) 
                {
                    printk( "AD malformed\n" );
                    return true;
                }
                struct bt_uuid* uuid;
                u8_t val[16];
                memcpy(val, data->data, 16);
                uuid = BT_UUID_DECLARE_128(val[0], val[1], val[2], val[3], val[4],
                        val[5], val[6], val[7], val[8], val[9],
                        val[10], val[11], val[12], val[13], val[14], val[15]);
                if (bt_uuid_cmp(uuid, (const struct bt_uuid *)&user_service_uuid)) {
                    return false;
                }
                int err = bt_le_scan_stop();
                if (err) {
                    printk("Stop LE scan failed (err %d)\n", err);
                    return false;
                }

                default_conn = bt_conn_create_le(addr, BT_LE_CONN_PARAM_DEFAULT);
                return false;
            }
    }

    return true;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
			 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
	       dev, type, ad->len, rssi);

	/* We're only interested in connectable events */
	if (type == BT_LE_ADV_IND || type == BT_LE_ADV_DIRECT_IND) {
		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason %u)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}


#if defined(CONFIG_BT_SMP)
static void identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
			      const bt_addr_le_t *identity)
{
	char addr_identity[BT_ADDR_LE_STR_LEN];
	char addr_rpa[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
	bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

	printk("Identity resolved %s -> %s\n", addr_rpa, addr_identity);
}

static void security_changed(struct bt_conn *conn, bt_security_t level)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Security changed: %s level %u\n", addr, level);

	if (level == BT_SECURITY_FIPS && conn == default_conn) {
        memcpy(&uuid, &user_service_uuid, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = 0x0001;
		discover_params.end_handle = 0xffff;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		int err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			printk("Discover failed(err %d)\n", err);
			return;
		}
	}
}
#endif /* defined(CONFIG_BT_SMP) */


static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_req       = NULL,
	.le_param_updated   = NULL,
#if defined(CONFIG_BT_SMP)
	.identity_resolved  = identity_resolved,
	.security_changed   = security_changed
#endif /* defined(CONFIG_BT_SMP) */
};

void main(void)
{
	int err;
	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

	if (err) {
		printk("[%s] Scanning failed to start (err %d)\n", __func__, err);
		return;
	}

	printk("Scanning successfully started\n");
}
