/**
 * Copyright (C) 2016 flexVDI (Flexible Software Solutions S.L.)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "glue-service.h"
#include "glue-connection.h"
#include "glib.h"
#include "usb-glue.h"
#include "usb-device-widget.h"
#include "spice-client.h"
#include "usb-device-manager.h"

#ifdef USBREDIR

#ifdef G_OS_WIN32
#include <windows.h>

#define FLEXVDI_MSGLOOP_WINCLASS_NAME  TEXT("FLEXVDI_MSGLOOP_CLIENT")

HWND hwnd = NULL;
#endif


SpiceUsbDeviceWidget *usbWidget = NULL;

/* Temporary storage for the list of UsbDeviceInfo
 * Safe to be called by client program thread 
 * - usbDevices: full list
 * - device: not yet retrieved list.
 */
GSList *devices;
GSList *device;

#ifdef G_OS_WIN32
static gboolean recv_windows_message (GIOChannel  *channel,
		      GIOCondition cond,
		      gpointer    data)
{
  GIOError error;
  MSG msg;
  guint nb;
  
  while (1)
    {
      error = g_io_channel_read (channel, &msg, sizeof (MSG), &nb);
      
      if (error != G_IO_ERROR_NONE)
   	  {
	      if (error == G_IO_ERROR_AGAIN)
	        continue;
	  }
      break;
    }
  
  return TRUE;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    /* Don care about the event. Just forward */
    return DefWindowProc(hwnd, message, wparam, lparam);
}

/* Called from glib mainloop thread */
static gboolean startMessageLoop() {

    WNDCLASS wcls;
    
    /* create a hidden window */
    memset(&wcls, 0, sizeof(wcls));
    wcls.lpfnWndProc = wnd_proc;
    wcls.lpszClassName = FLEXVDI_MSGLOOP_WINCLASS_NAME;
    if (!RegisterClass(&wcls)) {
        DWORD e = GetLastError();
        g_warning("RegisterClass failed , %ld", (long)e);
        return FALSE;
    }
    hwnd = CreateWindow(FLEXVDI_MSGLOOP_WINCLASS_NAME,
                              NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!hwnd) {
        DWORD e = GetLastError();
        g_warning("CreateWindow failed: %ld", (long)e);
        goto failed_unreg;
    }
    
    /* Create a message loop */
    GIOChannel *windows_messages_channel = g_io_channel_win32_new_messages((gsize)hwnd);
    g_io_add_watch(windows_messages_channel, G_IO_IN, recv_windows_message,0);
    return FALSE;

 failed_unreg:
    UnregisterClass(FLEXVDI_MSGLOOP_WINCLASS_NAME, NULL);
    
    return FALSE;    
}

void SpiceGlibGlue_InitWindowsEvents() {

    g_timeout_add_full(G_PRIORITY_HIGH, 0,
                       startMessageLoop,
                       NULL, NULL);
}

static gboolean finalizeMessageWindow() {
    if (hwnd) {
        DestroyWindow(hwnd);
        UnregisterClass(FLEXVDI_MSGLOOP_WINCLASS_NAME, NULL);
    }
    return FALSE;    
}

void SpiceGlibGlue_FinalizeWindowsEvents() {
    g_timeout_add_full(G_PRIORITY_HIGH, 0,
                       finalizeMessageWindow,
                       NULL, NULL);
}

#endif

void usb_glue_register_session(SpiceSession* session) {

    g_clear_object(&usbWidget);

    usbWidget = g_object_new(SPICE_TYPE_USB_DEVICE_WIDGET,
            "session", session,
            NULL);
}

void SpiceGlibGlue_GetUsbDeviceList() {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);
    if (!usbWidget) {
        g_error("Requested Usb device list  before initialization");
    }

    devices = spice_usb_device_widget_get_devices(usbWidget);
    device = devices;
    SPICE_DEBUG("USB: SpiceGlibGlueGetUsbDeviceList() END");
}

SpiceUsbDevice* SpiceGlibGlue_GetNextUsbDevice(char* devName, char* devId, 
        int32_t* isShared, int32_t* isEnabled, int32_t* opPending) {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);

    // When we get past the end of the list, free the list
    if (!device) {
        SPICE_DEBUG("USB: No more devices.");
        devName[0]= '\0';

        if (devices) {
            g_slist_free_full(devices, g_free);
            devices= NULL;
        }
        return (void *)NULL;
    } else {
        UsbDeviceInfo *dev = device->data;
        g_strlcpy(devName, dev->name, MAX_USB_DEVICE_NAME_SIZE);
        g_strlcpy(devId, dev->id, MAX_USB_DEVICE_ID_SIZE);
        *isShared = dev->isShared;
        *isEnabled = dev->isEnabled;
        *opPending = dev->isOpPending;
        SPICE_DEBUG("USB: Returning devName %s, isShared= %d, isEnabled = %d, isOpPending = %d", 
            devName, dev->isShared, dev->isEnabled, dev->isOpPending);
        device = g_slist_next(device);
        return dev->device;
    }
}

int32_t SpiceGlibGlue_IsUsbInitialized() {
    return usbWidget != NULL ? 1 : 0;
}

int32_t SpiceGlibGlue_isUsbDeviceListChanged() {
    if (!usbWidget) {
        g_error("Requested isUsbDeviceListChanged before initialization");
    }
    return spice_usb_device_widget_is_changed(usbWidget);
}

void SpiceGlibGlue_ShareUsbDevice(SpiceUsbDevice* d) {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);

    if (!usbWidget) {
        g_error("Requested UsbWidget before initialization");
    }
    
    spice_usb_device_widget_share(usbWidget, d);
}

void SpiceGlibGlue_UnshareUsbDevice(SpiceUsbDevice* d) {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);

    if (!usbWidget) {
        g_error("Requested UsbWidget before initialization");
    }
    
    spice_usb_device_widget_unshare(usbWidget, d);
}

void SpiceGlibGlue_GetUsbErrMsg(char* errMsg) {

    if (!usbWidget) {
        errMsg[0] = '\0';
        return;
    }
    spice_usb_device_widget_get_error_msg(usbWidget, errMsg);
}

int32_t SpiceGlibGlue_isUsbErrMsgChanged() {
    if (!usbWidget) {
        g_error("Requested isUsbErrMsgChanged before initialization");
    }
    return spice_usb_device_widget_is_msg_changed(usbWidget);
}

/* Mobile platform file descriptor-based USB redirection implementation */

static void set_usb_err_msg(char *errMsg, const char *msg)
{
    if (errMsg != NULL) {
        g_strlcpy(errMsg, msg, MAX_USB_ERR_MSG_SIZE);
    }
}

/* Resolves the manager for the current connection. `errMsg` and `conn_out` are optional. */
static SpiceUsbDeviceManager *usb_device_manager(SpiceConnection **conn_out, char *errMsg)
{
    SpiceConnection *conn = global_connection();
    if (conn == NULL) {
        const char *msg = "No active SPICE connection";
        g_warning("%s: %s", __func__, msg);
        set_usb_err_msg(errMsg, msg);
        return NULL;
    }

    SpiceSession *session = spice_connection_get_session(conn);
    if (session == NULL || !SPICE_IS_SESSION(session)) {
        const char *msg = "No SPICE session available";
        g_warning("%s: %s", __func__, msg);
        set_usb_err_msg(errMsg, msg);
        return NULL;
    }

    GError *err = NULL;
    SpiceUsbDeviceManager *manager = spice_usb_device_manager_get(session, &err);
    if (manager == NULL || err != NULL) {
        const char *msg = err ? err->message : "Failed to get USB device manager";
        g_warning("%s: %s", __func__, msg);
        set_usb_err_msg(errMsg, msg);
        if (err != NULL) {
            g_error_free(err);
        }
        return NULL;
    }

    if (conn_out != NULL) {
        *conn_out = conn;
    }
    return manager;
}

typedef struct {
    int32_t fileDescriptor;
} ConnectFdCbData;

static void report_fd_outcome(GError *err, ConnectFdCbData *data, const char *what)
{
    if (err != NULL) {
        g_warning("USB: failed to %s device on file descriptor %d: %s",
                  what, data->fileDescriptor, err->message);
        g_error_free(err);
    } else {
        SPICE_DEBUG("USB: %s device on file descriptor %d", what, data->fileDescriptor);
    }
    g_free(data);
}

static void connect_fd_cb(GObject *gobject, GAsyncResult *res, gpointer user_data)
{
    GError *err = NULL;
    spice_usb_device_manager_connect_device_finish(SPICE_USB_DEVICE_MANAGER(gobject), res, &err);
    report_fd_outcome(err, user_data, "redirected");
}

static void disconnect_fd_cb(GObject *gobject, GAsyncResult *res, gpointer user_data)
{
    GError *err = NULL;
    spice_usb_device_manager_disconnect_device_finish(SPICE_USB_DEVICE_MANAGER(gobject), res, &err);
    report_fd_outcome(err, user_data, "gave back");
}

/* The caller owns the returned device. */
static SpiceUsbDevice *allocate_device_for_fd(SpiceUsbDeviceManager *manager,
                                              int32_t fileDescriptor, char *errMsg)
{
    GError *err = NULL;
    SpiceUsbDevice *device = spice_usb_device_manager_allocate_device_for_file_descriptor(
        manager, fileDescriptor, &err);

    if (device == NULL || err != NULL) {
        const char *msg = err ? err->message : "Failed to allocate USB device from file descriptor";
        g_warning("%s: %s", __func__, msg);
        set_usb_err_msg(errMsg, msg);
        if (err != NULL) {
            g_error_free(err);
        }
        return NULL;
    }
    return device;
}

SpiceUsbDevice* SpiceGlibGlue_AllocateUsbDeviceForFileDescriptor(int32_t fileDescriptor, char* errMsg)
{
    g_debug(" %s:%d:%s() fileDescriptor=%d", __FILE__, __LINE__, __func__, fileDescriptor);

    SpiceUsbDeviceManager *manager = usb_device_manager(NULL, errMsg);
    if (manager == NULL) {
        return NULL;
    }
    return allocate_device_for_fd(manager, fileDescriptor, errMsg);
}

int32_t SpiceGlibGlue_AttachUsbDeviceByFileDescriptor(int32_t fileDescriptor, char* errMsg)
{
    g_debug(" %s:%d:%s() fileDescriptor=%d", __FILE__, __LINE__, __func__, fileDescriptor);

    SpiceConnection *conn = NULL;
    SpiceUsbDeviceManager *manager = usb_device_manager(&conn, errMsg);
    if (manager == NULL) {
        return 0;
    }

    SpiceUsbDevice *device = allocate_device_for_fd(manager, fileDescriptor, errMsg);
    if (device == NULL) {
        return 0;
    }

    ConnectFdCbData *cb_data = g_new(ConnectFdCbData, 1);
    cb_data->fileDescriptor = fileDescriptor;
    spice_usb_device_manager_connect_device_async(manager, device, NULL, connect_fd_cb, cb_data);

    g_hash_table_insert(spice_connection_get_usb_devices(conn),
                        GINT_TO_POINTER(fileDescriptor), device);
    return 1;
}

int32_t SpiceGlibGlue_DetachUsbDeviceByFileDescriptor(int32_t fileDescriptor, char* errMsg)
{
    g_debug(" %s:%d:%s() fileDescriptor=%d", __FILE__, __LINE__, __func__, fileDescriptor);

    SpiceConnection *conn = NULL;
    SpiceUsbDeviceManager *manager = usb_device_manager(&conn, errMsg);
    if (manager == NULL) {
        return 0;
    }

    GHashTable *usbDevices = spice_connection_get_usb_devices(conn);
    SpiceUsbDevice *device = g_hash_table_lookup(usbDevices, GINT_TO_POINTER(fileDescriptor));
    if (device == NULL) {
        const char *msg = "USB device not found for the given file descriptor";
        g_warning("%s: %s (fd=%d)", __func__, msg, fileDescriptor);
        set_usb_err_msg(errMsg, msg);
        return 0;
    }

    if (spice_usb_device_manager_is_device_connected(manager, device)) {
        ConnectFdCbData *cb_data = g_new(ConnectFdCbData, 1);
        cb_data->fileDescriptor = fileDescriptor;
        spice_usb_device_manager_disconnect_device_async(manager, device, NULL,
                                                         disconnect_fd_cb, cb_data);
    } else {
        SPICE_DEBUG("USB: device on file descriptor %d was not redirected", fileDescriptor);
    }

    g_hash_table_remove(usbDevices, GINT_TO_POINTER(fileDescriptor));
    return 1;
}


/* The widget holds a strong reference to the session, so nothing downstream of it is
 * released until this runs: the USB device manager, its libusb context, and every open
 * device handle. Devices must be disconnected while the manager is still alive. */
void usb_glue_release_session(void)
{
    SpiceSession *session = NULL;

    if (usbWidget != NULL) {
        g_object_get(usbWidget, "session", &session, NULL);
    } else {
        SpiceConnection *conn = global_connection();
        if (conn != NULL) {
            session = spice_connection_get_session(conn);
            if (session != NULL) {
                g_object_ref(session);
            }
        }
    }

    if (session != NULL) {
        GError *err = NULL;
        SpiceUsbDeviceManager *manager = spice_usb_device_manager_get(session, &err);

        if (manager != NULL && err == NULL) {
            GPtrArray *device_list = spice_usb_device_manager_get_devices(manager);
            if (device_list != NULL) {
                for (guint i = 0; i < device_list->len; i++) {
                    SpiceUsbDevice *dev = g_ptr_array_index(device_list, i);
                    if (spice_usb_device_manager_is_device_connected(manager, dev)) {
                        SPICE_DEBUG("USB: disconnecting device %d at session end", i);
                        spice_usb_device_manager_disconnect_device(manager, dev);
                    }
                }
                g_ptr_array_unref(device_list);
            }
        }
        if (err != NULL) {
            g_error_free(err);
        }
        g_object_unref(session);
    }

    g_clear_object(&usbWidget);
}

#endif
