/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

/** @file
 *
 */

#include <stdio.h>
#include "LinkListUtils.h"
#include "bt_smartbridge_socket_manager.h"

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

static bool smartbridge_socket_manager_find_socket_by_handle_callback  ( linked_list_node_t *node_to_compare,
                                                                         void *user_data );
static bool smartbridge_socket_manager_find_socket_by_address_callback ( linked_list_node_t *node_to_compare,
                                                                         void *user_data );

/******************************************************
 *               Variable Definitions
 ******************************************************/

/* Socket Management Globals */
static linked_list_t connected_socket_list;
static aos_mutex_t  connected_socket_list_mutex;
static uint8_t       max_number_of_connections = 0;

/******************************************************
 *               Function Definitions
 ******************************************************/

OSStatus bt_smartbridge_socket_manager_init( void )
{
    OSStatus result;

    result = linked_list_init( &connected_socket_list );
    if ( result != oNoErr ) {
        printf( ( "Error creating linked list\n" ) );
        return result;
    }

    result = aos_rtos_init_mutex( &connected_socket_list_mutex );
    if ( result != oNoErr ) {
        printf( ( "Error creating mutex\n" ) );
        return result;
    }

    max_number_of_connections = 1;

    return AOS_BT_SUCCESS;
}

OSStatus bt_smartbridge_socket_manager_deinit( void )
{
    aos_rtos_deinit_mutex( &connected_socket_list_mutex );
    linked_list_deinit( &connected_socket_list );
    max_number_of_connections = 0;
    return AOS_BT_SUCCESS;
}

OSStatus bt_smartbridge_socket_manager_set_max_concurrent_connections( uint8_t count )
{
    max_number_of_connections = count;
    return AOS_BT_SUCCESS;
}

aos_bool_t   bt_smartbridge_socket_manager_is_full( void )
{
    uint32_t active_connection_count;

    linked_list_get_count( &connected_socket_list, &active_connection_count );

    return ( active_connection_count == max_number_of_connections ) ? AOS_TRUE : AOS_FALSE;
}

OSStatus bt_smartbridge_socket_manager_insert_socket( aos_bt_smartbridge_socket_t *socket )
{
    OSStatus  result;
    uint32_t  count;

    linked_list_get_count( &connected_socket_list, &count );

    if ( count == max_number_of_connections ) {
        return AOS_BT_MAX_CONNECTIONS_REACHED;
    }

    /* Lock protection */
    aos_rtos_lock_mutex( &connected_socket_list_mutex );

    result = linked_list_insert_node_at_rear( &connected_socket_list, &socket->node );

    /* Unlock protection */
    aos_rtos_unlock_mutex( &connected_socket_list_mutex );

    return result;
}

OSStatus bt_smartbridge_socket_manager_remove_socket( uint16_t connection_handle, aos_bt_smartbridge_socket_t **socket )
{
    OSStatus            result;
    uint32_t            count;
    linked_list_node_t *node_found;
    uint32_t            user_data = connection_handle;

    linked_list_get_count( &connected_socket_list, &count );

    if ( count == 0 ) {
        return AOS_BT_SOCKET_NOT_CONNECTED;
    }

    /* Lock protection */
    aos_rtos_lock_mutex( &connected_socket_list_mutex );

    result = linked_list_find_node( &connected_socket_list, smartbridge_socket_manager_find_socket_by_handle_callback,
                                    (void *)user_data, &node_found );
    if ( result == AOS_BT_SUCCESS ) {
        result = linked_list_remove_node( &connected_socket_list, node_found );

        if ( result == AOS_BT_SUCCESS ) {
            *socket = (aos_bt_smartbridge_socket_t *)node_found->data;
        }
    }

    /* Unlock protection */
    aos_rtos_unlock_mutex( &connected_socket_list_mutex );

    return result;
}

OSStatus bt_smartbridge_socket_manager_find_socket_by_handle( uint16_t connection_handle,
                                                              aos_bt_smartbridge_socket_t **socket )
{
    OSStatus            result;
    uint32_t            count;
    linked_list_node_t *node_found;
    uint32_t            user_data = connection_handle;

    linked_list_get_count( &connected_socket_list, &count );

    if ( count == 0 ) {
        return AOS_BT_SOCKET_NOT_CONNECTED;
    }

    /* Lock protection */
    aos_rtos_lock_mutex( &connected_socket_list_mutex );

    result = linked_list_find_node( &connected_socket_list, smartbridge_socket_manager_find_socket_by_handle_callback,
                                    (void *)user_data, &node_found );

    if ( result == AOS_BT_SUCCESS ) {
        *socket = (aos_bt_smartbridge_socket_t *)node_found->data;
    }

    /* Unlock protection */
    aos_rtos_unlock_mutex( &connected_socket_list_mutex );

    return result;
}

OSStatus bt_smartbridge_socket_manager_find_socket_by_address( const aos_bt_device_address_t *address,
                                                               aos_bt_smartbridge_socket_t **socket )
{
    OSStatus            result;
    uint32_t            count;
    linked_list_node_t *node_found;

    linked_list_get_count( &connected_socket_list, &count );

    if ( count == 0 ) {
        return AOS_BT_SOCKET_NOT_CONNECTED;
    }

    /* Lock protection */
    aos_rtos_lock_mutex( &connected_socket_list_mutex );

    result = linked_list_find_node( &connected_socket_list, smartbridge_socket_manager_find_socket_by_address_callback,
                                    (void *)address, &node_found );

    if ( result == AOS_BT_SUCCESS ) {
        *socket = (aos_bt_smartbridge_socket_t *)node_found->data;
    }

    /* Unlock protection */
    aos_rtos_unlock_mutex( &connected_socket_list_mutex );

    return result;
}

static bool smartbridge_socket_manager_find_socket_by_handle_callback( linked_list_node_t *node_to_compare,
                                                                       void *user_data )
{
    aos_bt_smartbridge_socket_t *socket = (aos_bt_smartbridge_socket_t *)node_to_compare->data;
    uint32_t connection_handle = (uint32_t)user_data;

    return ( socket->connection_handle == connection_handle ) ? true : false;
}

static bool smartbridge_socket_manager_find_socket_by_address_callback( linked_list_node_t *node_to_compare,
                                                                        void *user_data )
{
    aos_bt_smartbridge_socket_t *socket = (aos_bt_smartbridge_socket_t *)node_to_compare->data;
    aos_bt_device_address_t      *address = (aos_bt_device_address_t *)user_data;

    return ( memcmp( socket->remote_device.address, address, sizeof( *address ) ) == 0 ) ? true : false;
}