/**
 * Copyright (C) 2016 flexVDI (Flexible Software Solutions S.L.)
 * Copyright (C) 2013 Iordan Iordanov
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __SPICE_WIDGET_PRIV_H__
#define __SPICE_WIDGET_PRIV_H__

G_BEGIN_DECLS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_X11
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <gdk/gdkx.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include <spice-session.h>

#include "spice-grabsequence.h"
#include "mono-glue-types.h"

#define SPICE_DISPLAY_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_DISPLAY, SpiceDisplayPrivate))


struct _SpiceDisplayPrivate {
    gint                    channel_id;
    gint                    monitor_id;

    /* options */
    gboolean                keyboard_grab_inhibit;
    gboolean                    resize_guest_enable;

    /* state */
    gboolean                ready;
    gboolean                monitor_ready;
    enum SpiceSurfaceFmt    format;
    gint                    width, height, stride;
    gint                    shmid;
    gpointer                data_origin; /* the original display image data */
    gpointer                data; /* converted if necessary to 32 bits */

     /* current display buffer size */
    uint32_t                disp_buffer_width;
    uint32_t                disp_buffer_height;

    gboolean                    convert;
    gboolean                disable_inputs;

    SpiceSession            *session;
    SpiceMainChannel        *main;
    SpiceChannel            *display;
    SpiceCursorChannel      *cursor;
    SpiceInputsChannel      *inputs;
    SpiceSmartcardChannel   *smartcard;

    enum SpiceMouseMode     mouse_mode;
    int                     mouse_grab_active;
    
    /* MUTEX to control cursor access */
    GMutex            cursor_lock;
    /* Client mode mouse, for mono client */
    MonoGlueCursor	    *mouse_cursor;
    /* Internal cursor identification */
    guint32                 idCursor;
    /* Hidden cursor, for mono client */
    MonoGlueCursor          *show_cursor;

    int                     mouse_last_x;
    int                     mouse_last_y;
    /* Mouse position in "server mode" */
    int                     mouse_guest_x;
    int                     mouse_guest_y;

    gboolean                    keyboard_grab_active;
    gboolean                    have_focus;

    const guint16           *keycode_map;
    size_t                  keycode_maplen;
    uint32_t                key_state[512 / 32];
    int                     key_delayed_scancode;
    guint                   key_delayed_id;
    SpiceGrabSequence       *grabseq; /* the configured key sequence */
    gboolean                *activeseq; /* the currently pressed keys */
    gint                    mark;
#ifdef WIN32
    HHOOK                   keyboard_hook;
    int                     win_mouse[3];
    int                     win_mouse_speed;
#endif
    guint                   keypress_delay;
    gint                    zoom_level;

    gboolean                invalidated;
    gint                    invalidate_x;
    gint                    invalidate_y;
    gint                    invalidate_w;
    gint                    invalidate_h;
    int                     copy_scheduled;

    gboolean                updatedDisplayBuffer;

    void (*buffer_resize_callback)(int, int, int);
    void (*buffer_update_callback)(int, int, int, int);
    void (*cursor_shape_updated_callback)(int width, int height, int x, int y, int *pixels);
};

G_END_DECLS

#endif
