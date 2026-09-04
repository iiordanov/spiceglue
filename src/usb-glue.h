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
 
/*
 * Simple functions to use usb redirection in spice client programs.
 */

#ifdef USBREDIR
#ifndef _GLUE_USB_H
#define _GLUE_USB_H

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#include "glue-spice-widget.h"

/* Called internally by spiceglue during initialization.
 */
void usb_glue_register_session(SpiceSession* session);
void usb_glue_release_session(void);

/* File-descriptor USB redirection, for a platform whose OS hands out a file descriptor
 * for a USB device. Mac Catalyst names devices by io_service_t instead; see below.
 *
 * Every errMsg argument in this file may be NULL, and is written with at most
 * MAX_USB_ERR_MSG_SIZE bytes including the terminator.
 */

/*
 * Allocate a USB device from a file descriptor obtained through the OS's USB permission
 * mechanism. Returns NULL on failure. The caller owns the returned device and there is no
 * entry point that frees it, so a caller that only wants the device redirected should call
 * SpiceGlibGlue_AttachUsbDeviceByFileDescriptor, which allocates one itself.
 */
SpiceUsbDevice* SpiceGlibGlue_AllocateUsbDeviceForFileDescriptor(int32_t fileDescriptor, char* errMsg);

/*
 * Attach a USB device to the SPICE session using a file descriptor.
 * This is the mobile-specific version that accepts a file descriptor instead
 * of relying on libusb device enumeration.
 * Returns true on success, false on failure.
 */
int32_t SpiceGlibGlue_AttachUsbDeviceByFileDescriptor(int32_t fileDescriptor, char* errMsg);

/*
 * Detach a USB device from the SPICE session using its file descriptor.
 * Returns true on success, false on failure.
 */
int32_t SpiceGlibGlue_DetachUsbDeviceByFileDescriptor(int32_t fileDescriptor, char* errMsg);

/* Create an internal list of  connected usbDevices to be retrieved by one by 
 * one by SpiceGlibGlueGetNextUsbDevice()
 */
void SpiceGlibGlue_GetUsbDeviceList();

/* 
 * Copies to devName the name of the next device in the list.
 * The char* passed in "devName" must provide capacity of at least 
 * MAX_USB_DEVICE_NAME_SIZE bytes, which is the limit that we artificially impose.
 * The invocation after the last device frees the list and passes back an empty ("")
 * string; a caller that stops before then leaks it, as does calling
 * SpiceGlibGlue_GetUsbDeviceList twice.
 * - devId: at least MAX_USB_DEVICE_ID_SIZE bytes.
 * - isShared: true if the device is currently shared with the guest.
 */
SpiceUsbDevice* SpiceGlibGlue_GetNextUsbDevice(char* devName, char* devId, 
        int32_t* isShared, int32_t* isEnabled, int32_t* opPending);

/*
 * Returns true if the device widget exists, which is what the functions below it need. It
 * is never created on Mac Catalyst, whose io_service_t entry points do not use it.
 */
int32_t SpiceGlibGlue_IsUsbInitialized();

/*
 * Returns true if the usbDevice List has changed since the last time SpiceGlibGlueGetUsbDeviceList
 * was called
 */
int32_t SpiceGlibGlue_isUsbDeviceListChanged();


void SpiceGlibGlue_ShareUsbDevice(SpiceUsbDevice* d);
void SpiceGlibGlue_UnshareUsbDevice(SpiceUsbDevice* d);


/*
 * Returns true if the error message has changed since SpiceGlibGlue_GetUsbErrMsg
 * last read it.
 */
int32_t SpiceGlibGlue_isUsbErrMsgChanged();

/* Copies the current error message into errMsg, which must provide at least
 * MAX_USB_ERR_MSG_SIZE bytes. Clears the changed flag. */
void SpiceGlibGlue_GetUsbErrMsg(char* errMsg);

void SpiceGlibGlue_InitWindowsEvents();
void SpiceGlibGlue_FinalizeWindowsEvents();

#endif /* _GLUE_USB_H */
#endif
