/*
 * Wazuh INVENTORY
 * Copyright (C) 2015, Wazuh Inc.
 * November 11, 2021.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */
#include <stdlib.h>
#include "../../wmodules_def.h"
#include "wmodules.h"
#include "wm_inventory.h"
#include "inventory.h"
#include "sym_load.h"
#include "defs.h"
#include "mq_op.h"
#include "headers/logging_helper.h"
#include "commonDefs.h"

#ifdef WIN32
static DWORD WINAPI wm_inv_main(void *arg);         // Module main function. It won't return
#else
static void* wm_inv_main(wm_inv_t *inv);        // Module main function. It won't return
#endif
static void wm_inv_destroy(wm_inv_t *data);      // Destroy data
static void wm_inv_stop(wm_inv_t *inv);         // Module stopper
const char *WM_INV_LOCATION = "inventory";   // Location field for event sending
cJSON *wm_inv_dump(const wm_inv_t *inv);
int wm_inv_message(const char *data);

const wm_context WM_INV_CONTEXT = {
    .name = "inventory",
    .start = (wm_routine)wm_inv_main,
    .destroy = (void(*)(void *))wm_inv_destroy,
    .dump = (cJSON * (*)(const void *))wm_inv_dump,
    .sync = (int(*)(const char*))wm_sync_message,
    .stop = (void(*)(void *))wm_inv_stop,
    .query = NULL,
};

void *inventory_module = NULL;
inventory_start_func inventory_start_ptr = NULL;
inventory_stop_func inventory_stop_ptr = NULL;
inventory_sync_message_func inventory_sync_message_ptr = NULL;

#ifndef CLIENT
void *router_module_ptr = NULL;
router_provider_create_func router_provider_create_func_ptr = NULL;
router_provider_send_fb_func router_provider_send_fb_func_ptr = NULL;
ROUTER_PROVIDER_HANDLE rsync_handle = NULL;
ROUTER_PROVIDER_HANDLE inventory_handle = NULL;
int disable_manager_scan = 1;
#endif // CLIENT

long inventory_sync_max_eps = 10;    // Database synchronization number of events per second (default value)
int queue_fd = 0;                       // Output queue file descriptor

static bool is_shutdown_process_started() {
    bool ret_val = shutdown_process_started;
    return ret_val;
}

#ifdef WIN32
DWORD WINAPI wm_inv_main(void *arg) {
    wm_inv_t *inv = (wm_inv_t *)arg;
#else
void* wm_inv_main(wm_inv_t *inv) {
#endif
    w_cond_init(&inv_stop_condition, NULL);
    w_mutex_init(&inv_stop_mutex, NULL);
    w_mutex_init(&inv_reconnect_mutex, NULL);

    if (!inv->flags.enabled) {
        mtinfo(WM_INV_LOGTAG, "Module disabled. Exiting...");
        pthread_exit(NULL);
    }

    #ifndef WIN32
    // Connect to socket
    queue_fd = StartMQ(DEFAULTQUEUE, WRITE, INFINITE_OPENQ_ATTEMPTS);

    if (queue_fd < 0) {
        mterror(WM_INV_LOGTAG, "Can't connect to queue.");
        pthread_exit(NULL);
    }
    #endif

    if (inventory_module = so_get_module_handle("inventory"), inventory_module)
    {
        inventory_start_ptr = so_get_function_sym(inventory_module, "inventory_start");
        inventory_stop_ptr = so_get_function_sym(inventory_module, "inventory_stop");
        inventory_sync_message_ptr = so_get_function_sym(inventory_module, "inventory_sync_message");

        void* rsync_module = NULL;
        if(rsync_module = so_check_module_loaded("rsync"), rsync_module) {
            rsync_initialize_full_log_func rsync_initialize_log_function_ptr = so_get_function_sym(rsync_module, "rsync_initialize_full_log_function");
            if(rsync_initialize_log_function_ptr) {
                rsync_initialize_log_function_ptr(mtLoggingFunctionsWrapper);
            }
            // Even when the RTLD_NOLOAD flag was used for dlopen(), we need a matching call to dlclose()
#ifndef WIN32
            so_free_library(rsync_module);
#endif
        }
    } else {
#ifdef __hpux
        mtinfo(WM_INV_LOGTAG, "Not supported in HP-UX.");
#else
        mterror(WM_INV_LOGTAG, "Can't load inventory.");
#endif
        pthread_exit(NULL);
    }
    if (inventory_start_ptr) {
        mtdebug1(WM_INV_LOGTAG, "Starting inventory.");
        w_mutex_lock(&inv_stop_mutex);
        need_shutdown_wait = true;
        w_mutex_unlock(&inv_stop_mutex);
        const long max_eps = inv->sync.sync_max_eps;
        if (0 != max_eps) {
            inventory_sync_max_eps = max_eps;
        }
        // else: if max_eps is 0 (from configuration) let's use the default max_eps value (10)
        wm_inv_log_config(inv);

        inventory_start_ptr(inv->interval,
                               wm_inv_send_diff_message,
                               wm_inv_send_dbsync_message,
                               taggedLogFunction,
                               INVENTORY_DB_DISK_PATH,
                               INVENTORY_NORM_CONFIG_DISK_PATH,
                               INVENTORY_NORM_TYPE,
                               inv->flags.scan_on_start,
                               inv->flags.hwinfo,
                               inv->flags.osinfo,
                               inv->flags.netinfo,
                               inv->flags.programinfo,
                               inv->flags.portsinfo,
                               inv->flags.allports,
                               inv->flags.procinfo,
                               inv->flags.hotfixinfo);
    } else {
        mterror(WM_INV_LOGTAG, "Can't get inventory_start_ptr.");
        pthread_exit(NULL);
    }
    inventory_sync_message_ptr = NULL;
    inventory_start_ptr = NULL;
    inventory_stop_ptr = NULL;

    if (queue_fd) {
        close(queue_fd);
        queue_fd = 0;
    }
    so_free_library(inventory_module);

    inventory_module = NULL;
    mtinfo(WM_INV_LOGTAG, "Module finished.");
    w_mutex_lock(&inv_stop_mutex);
    w_cond_signal(&inv_stop_condition);
    w_mutex_unlock(&inv_stop_mutex);
    return 0;
}

void wm_inv_destroy(wm_inv_t *data) {
    free(data);
}

void wm_inv_stop(__attribute__((unused))wm_inv_t *data) {
    mtinfo(WM_INV_LOGTAG, "Stop received for Inventory.");
    inventory_sync_message_ptr = NULL;
    if (inventory_stop_ptr){
        shutdown_process_started = true;
        inventory_stop_ptr();
    }
    w_mutex_lock(&inv_stop_mutex);
    if (need_shutdown_wait) {
        w_cond_wait(&inv_stop_condition, &inv_stop_mutex);
    }
    w_mutex_unlock(&inv_stop_mutex);

    w_cond_destroy(&inv_stop_condition);
    w_mutex_destroy(&inv_stop_mutex);
    w_mutex_destroy(&inv_reconnect_mutex);
}

int wm_sync_message(const char *data)
{
    int ret_val = 0;

    if (inventory_sync_message_ptr) {
        ret_val = inventory_sync_message_ptr(data);
    }

    return ret_val;
}
