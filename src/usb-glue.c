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

#if defined(__APPLE__) && TARGET_OS_MACCATALYST
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#endif
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

#if defined(__APPLE__) && TARGET_OS_MACCATALYST
static void clear_redirect_states(void);
#endif

void usb_glue_register_session(SpiceSession* session) {

#if defined(__APPLE__) && TARGET_OS_MACCATALYST
    clear_redirect_states();
#endif

    g_clear_object(&usbWidget);

#if defined(__APPLE__) && TARGET_OS_MACCATALYST
    /* The device list the widget maintains has no reader on this platform. */
    (void)session;
#else
    usbWidget = g_object_new(SPICE_TYPE_USB_DEVICE_WIDGET,
            "session", session,
            NULL);
#endif
}

void SpiceGlibGlue_GetUsbDeviceList() {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);
    if (!usbWidget) {
        g_warning("%s: no USB device widget on this platform", __func__);
        devices = NULL;
        device = NULL;
        return;
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
        return 0;
    }
    return spice_usb_device_widget_is_changed(usbWidget);
}

void SpiceGlibGlue_ShareUsbDevice(SpiceUsbDevice* d) {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);

    if (!usbWidget) {
        g_warning("%s: no USB device widget on this platform", __func__);
        return;
    }
    
    spice_usb_device_widget_share(usbWidget, d);
}

void SpiceGlibGlue_UnshareUsbDevice(SpiceUsbDevice* d) {

    g_debug(" %s:%d:%s()", __FILE__, __LINE__, __func__);

    if (!usbWidget) {
        g_warning("%s: no USB device widget on this platform", __func__);
        return;
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
        return 0;
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

/* macCatalyst IOKit-based USB support using io_service_t */
#if defined(__APPLE__) && TARGET_OS_MACCATALYST

#include <libusb.h>


/* bus number of a Darwin locationID */
#define DARWIN_LOCATION_TO_BUS(location) ((uint8_t)((location) >> 24))

static gboolean read_ioregistry_number(uint32_t io_service, CFStringRef key,
                                       CFNumberType type, void *out)
{
    CFTypeRef ref = IORegistryEntryCreateCFProperty(io_service, key,
                                                    kCFAllocatorDefault, 0);
    if (ref == NULL) {
        return FALSE;
    }
    gboolean ok = FALSE;
    if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
        ok = CFNumberGetValue((CFNumberRef)ref, type, out);
    }
    CFRelease(ref);
    return ok;
}

/* A device keeps its locationID across the re-enumeration that USB capture performs. */
static gboolean device_location_id(uint32_t io_service, UInt32 *location_id)
{
    if (!read_ioregistry_number(io_service, CFSTR("locationID"),
                                kCFNumberSInt32Type, location_id)) {
        g_warning("IOKit: Failed to read locationID for io_service 0x%x", io_service);
        return FALSE;
    }
    return TRUE;
}

/* The redirect outcome arrives on the glue main loop and is read from other threads. */
typedef struct {
    gboolean shared;
    gchar *error;
} RedirectState;

static GHashTable *redirectStates = NULL;   /* locationID -> RedirectState* */
static GMutex redirectStatesLock;

static void free_redirect_state(gpointer data)
{
    RedirectState *state = data;
    if (state) {
        g_free(state->error);
        g_free(state);
    }
}

static void set_redirect_state(uint32_t locationID, gboolean shared, const gchar *error)
{
    g_mutex_lock(&redirectStatesLock);
    if (redirectStates == NULL) {
        redirectStates = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                               NULL, free_redirect_state);
    }
    RedirectState *state = g_hash_table_lookup(redirectStates, GUINT_TO_POINTER(locationID));
    if (state == NULL) {
        state = g_new0(RedirectState, 1);
        g_hash_table_insert(redirectStates, GUINT_TO_POINTER(locationID), state);
    }
    state->shared = shared;
    g_free(state->error);
    state->error = error ? g_strdup(error) : NULL;
    g_mutex_unlock(&redirectStatesLock);
}

static void clear_redirect_states(void)
{
    g_mutex_lock(&redirectStatesLock);
    if (redirectStates != NULL) {
        g_hash_table_remove_all(redirectStates);
    }
    g_mutex_unlock(&redirectStatesLock);
}

static void redirect_cb(GObject *gobject, GAsyncResult *res, gpointer user_data)
{
    SpiceUsbDeviceManager *manager = SPICE_USB_DEVICE_MANAGER(gobject);
    uint32_t locationID = GPOINTER_TO_UINT(user_data);
    GError *err = NULL;

    spice_usb_device_manager_connect_device_finish(manager, res, &err);

    if (err != NULL) {
        g_warning("USB: redirection of device at locationID 0x%x failed: %s",
                  locationID, err->message);
        set_redirect_state(locationID, FALSE, err->message);
        g_error_free(err);
    } else {
        SPICE_DEBUG("USB: device at locationID 0x%x redirected", locationID);
        set_redirect_state(locationID, TRUE, NULL);
    }
}


static int16_t disable_auto_connect_on_main_loop(gpointer data)
{
    (void)data;
    SpiceConnection *conn = global_connection();
    if (conn == NULL) {
        g_warning("%s: No active SPICE connection", __func__);
        return 0;
    }

    SpiceSession *session = spice_connection_get_session(conn);
    if (session == NULL) {
        g_warning("%s: No SPICE session available", __func__);
        return 0;
    }

    GError *err = NULL;
    SpiceUsbDeviceManager *manager = spice_usb_device_manager_get(session, &err);

    if (manager == NULL || err != NULL) {
        g_warning("%s: Failed to get USB device manager: %s", __func__, err ? err->message : "unknown");
        if (err != NULL) g_error_free(err);
        return 0;
    }

    g_object_set(manager,
                 "auto-connect", FALSE,
                 "auto-connect-filter", "-1,-1,-1,-1,0",
                 NULL);
    SPICE_DEBUG("USB: disabled auto-connect and set filter to block all devices");
    return 1;
}

/* The manager owns the returned device, which is valid only for this call. */
static SpiceUsbDevice *find_spice_device_for_service(SpiceUsbDeviceManager *manager,
                                                     uint32_t io_service,
                                                     UInt32 *locationID_out)
{
    UInt16 vid = 0;
    if (!read_ioregistry_number(io_service, CFSTR("idVendor"), kCFNumberSInt16Type, &vid)) {
        g_warning("IOKit: Failed to get VID from IORegistry");
        return NULL;
    }

    UInt16 pid = 0;
    if (!read_ioregistry_number(io_service, CFSTR("idProduct"), kCFNumberSInt16Type, &pid)) {
        g_warning("IOKit: Failed to get PID from IORegistry");
        return NULL;
    }

    UInt32 locationID = 0;
    if (!device_location_id(io_service, &locationID)) {
        return NULL;
    }
    if (locationID_out != NULL) {
        *locationID_out = locationID;
    }

    UInt32 address = 0;
    gboolean have_address = read_ioregistry_number(io_service, CFSTR("USB Address"),
                                                   kCFNumberSInt32Type, &address);

    uint8_t wanted_bus = DARWIN_LOCATION_TO_BUS(locationID);
    uint8_t wanted_address = (uint8_t)address;

    SPICE_DEBUG("USB: Looking for %04x:%04x at bus %u address %u (have_address %d, locationID 0x%x)",
                vid, pid, wanted_bus, wanted_address, have_address, locationID);

    GPtrArray *device_list = spice_usb_device_manager_get_devices(manager);
    SPICE_DEBUG("USB: manager has %d devices enumerated", device_list ? device_list->len : 0);

    SpiceUsbDevice *spice_device = NULL;
    SpiceUsbDevice *fallback_device = NULL;
    gboolean ambiguous_fallback = FALSE;

    if (device_list) {
        for (guint i = 0; i < device_list->len; i++) {
            SpiceUsbDevice *dev = g_ptr_array_index(device_list, i);
            gconstpointer libusb_dev = spice_usb_device_get_libusb_device(dev);
            if (libusb_dev == NULL) {
                continue;
            }

            struct libusb_device_descriptor desc_struct;
            if (libusb_get_device_descriptor((libusb_device *)libusb_dev, &desc_struct) != 0) {
                continue;
            }

            uint8_t bus = libusb_get_bus_number((libusb_device *)libusb_dev);
            uint8_t addr = libusb_get_device_address((libusb_device *)libusb_dev);
            SPICE_DEBUG("USB: device #%d is %04x:%04x at bus %u address %u",
                        i, desc_struct.idVendor, desc_struct.idProduct, bus, addr);

            if (desc_struct.idVendor != vid || desc_struct.idProduct != pid ||
                bus != wanted_bus) {
                continue;
            }

            if (have_address && addr == wanted_address) {
                spice_device = dev;
                SPICE_DEBUG("USB: matched device #%d", i);
                break;
            }
            if (fallback_device == NULL) {
                fallback_device = dev;
            } else {
                ambiguous_fallback = TRUE;
            }
        }

        if (spice_device == NULL && fallback_device != NULL && !ambiguous_fallback) {
            SPICE_DEBUG("USB: no address match; using the only %04x:%04x on bus %u",
                        vid, pid, wanted_bus);
            spice_device = fallback_device;
        }

        g_ptr_array_unref(device_list);
    }

    if (spice_device == NULL) {
        g_warning("IOKit: %04x:%04x at bus %u address %u not found in manager's device list",
                  vid, pid, wanted_bus, wanted_address);
    }
    return spice_device;
}

static int16_t attach_usb_device_on_main_loop(gpointer data)
{
    uint32_t io_service = *(uint32_t *)data;
    g_debug(" %s:%d:%s() io_service=0x%x", __FILE__, __LINE__, __func__, io_service);

    SpiceUsbDeviceManager *manager = usb_device_manager(NULL, NULL);
    if (manager == NULL) {
        return 0;
    }

    disable_auto_connect_on_main_loop(NULL);

    UInt32 locationID = 0;
    SpiceUsbDevice *spice_device = find_spice_device_for_service(manager, io_service,
                                                                 &locationID);
    if (spice_device == NULL) {
        return 0;
    }

    set_redirect_state(locationID, FALSE, NULL);
    spice_usb_device_manager_connect_device_async(
        manager, spice_device, NULL, redirect_cb, GUINT_TO_POINTER(locationID));

    SPICE_DEBUG("USB: requested redirection of device at locationID 0x%x", locationID);
    return 1;
}

static int16_t detach_usb_device_on_main_loop(gpointer data)
{
    uint32_t io_service = *(uint32_t *)data;
    g_debug(" %s:%d:%s() io_service=0x%x", __FILE__, __LINE__, __func__, io_service);

    SpiceUsbDeviceManager *manager = usb_device_manager(NULL, NULL);
    if (manager == NULL) {
        return 0;
    }

    UInt32 locationID = 0;
    if (!device_location_id(io_service, &locationID)) {
        return 0;
    }
    set_redirect_state(locationID, FALSE, NULL);

    SpiceUsbDevice *spice_device = find_spice_device_for_service(manager, io_service, NULL);
    if (spice_device == NULL) {
        return 0;
    }

    if (spice_usb_device_manager_is_device_connected(manager, spice_device)) {
        spice_usb_device_manager_disconnect_device_async(manager, spice_device, NULL, NULL, NULL);
        SPICE_DEBUG("USB: requested end of redirection for device at locationID 0x%x", locationID);
    }

    return 1;
}

int32_t SpiceGlibGlue_GetUsbRedirectionState(uint32_t io_service, char *errMsg,
                                            int32_t errMsgSize)
{
    UInt32 locationID = 0;

    if (errMsg != NULL && errMsgSize > 0) {
        errMsg[0] = '\0';
    }
    if (!device_location_id(io_service, &locationID)) {
        return 0;
    }

    int32_t shared = 0;
    g_mutex_lock(&redirectStatesLock);
    if (redirectStates != NULL) {
        RedirectState *state = g_hash_table_lookup(redirectStates,
                                                   GUINT_TO_POINTER(locationID));
        if (state != NULL) {
            shared = state->shared ? 1 : 0;
            if (errMsg != NULL && errMsgSize > 0 && state->error != NULL) {
                g_strlcpy(errMsg, state->error, (gsize)errMsgSize);
            }
        }
    }
    g_mutex_unlock(&redirectStatesLock);

    return shared;
}

/* spice-gtk is not thread safe, so the two entry points below hand their work to the glue
 * main loop and wait for it. */

int32_t SpiceGlibGlue_AttachUsbDeviceByService(uint32_t io_service)
{
    return glue_call_on_main_loop(attach_usb_device_on_main_loop, &io_service) == 1;
}

int32_t SpiceGlibGlue_DetachUsbDeviceByService(uint32_t io_service)
{
    return glue_call_on_main_loop(detach_usb_device_on_main_loop, &io_service) == 1;
}

#endif /* defined(__APPLE__) && TARGET_OS_MACCATALYST */

/* Whatever holds the session keeps everything downstream of it alive: the USB device
 * manager, its libusb context, and every open device handle. Devices must be disconnected
 * while the manager is still alive. */
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

#if defined(__APPLE__) && TARGET_OS_MACCATALYST
    /* No redirect outcome outlives the session that produced it. */
    clear_redirect_states();
#endif

    g_clear_object(&usbWidget);
}

#endif
