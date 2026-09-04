/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "glue-service.h"
#include "glib.h"
#include "glue-spice-widget.h"
#include "glue-spice-widget-priv.h"
#include "glue-clipboard.h"
#include "glue-clipboard-client.h"

gboolean enableClipboardToGuest = FALSE;
gboolean enableClipboardToClient = FALSE;
uint32_t *guestClipboard = NULL;
uint32_t *hostClipboard = NULL;
void (*clientClipboardCallback)(char *);

/*  
 *   Clipboard sharing between client and guest, using spice client library
 *   and windows native API. No GTK or any other GUI framework.
 *
 *   Only utf-8 text implemented (although library supports images)
 *   ClipboardMain shared (but not primary clipboard, as windows has only the main one).
 */


/* Array with the type of data we are interested in sharing */
guint32 clipboardTypes [1] = {VD_AGENT_CLIPBOARD_UTF8_TEXT};
int ntypes = 1;

#define CB_OWNER_NONE 0
#define CB_OWNER_GUEST 1
#define CB_OWNER_HOST 2
int clipboardOwner = CB_OWNER_NONE;
int pendingGuestData = 0;

/* Values for comunication beween signal receiving thread 
 * and windows message receiving thread 
 */
gpointer current_data = NULL;
GMutex data_mutex;

//If more clipboard formats are to be supported, use CBData
typedef struct
{
    gpointer selection_data;
    guint selection_size;
    guint type;
} CBData;

void
push_clipboard_data (const guchar *data, guint size)
{
  g_mutex_lock (&data_mutex);
  SPICE_DEBUG("CB: data_mutex locked in push.\n");

  if (size >= CB_SIZE) {
      size = CB_SIZE - 1;
  }

  if (guestClipboard != NULL) {
      memcpy(guestClipboard, data, size);
      ((char *) guestClipboard)[size] = '\0';
  }

  pendingGuestData = 1;

  g_mutex_unlock (&data_mutex);
  SPICE_DEBUG("CB: data_mutex UNlocked in push.\n");
}

static gboolean grab_guest_clipboard(gpointer data)
{
    SpiceDisplay *display;
    SpiceDisplayPrivate *d;
    
    display = global_display();
    if (global_display() == NULL) {
        return FALSE;
    }

    d = SPICE_DISPLAY_GET_PRIVATE(display);
    if (d->main == NULL) {
        return FALSE;
    }
   
    /* Grab the guest clipboard, with just one type (text) */
    spice_main_clipboard_selection_grab(d->main, 
        VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD,
        clipboardTypes, ntypes);
    clipboardOwner = CB_OWNER_HOST;

    return FALSE;
}

int SpiceGlibGlue_GrabGuestClipboard()
{
    SPICE_DEBUG("CB: GrabGuestClipboard grabbing %d", VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD);
    
    if (!enableClipboardToGuest) {
        SPICE_DEBUG("CB: enableClipboardToGuest set to false. Doing nothing.");
        return 0;
    }

    g_idle_add(grab_guest_clipboard, NULL);

    return 0;
}

static gboolean release_guest_clipboard(gpointer data)
{
    SpiceDisplay *display;
    SpiceDisplayPrivate *d;
    
    display = global_display();
    if (global_display() == NULL) {
        return FALSE;
    }

    d = SPICE_DISPLAY_GET_PRIVATE(display);
    if (d->main == NULL) {
        return FALSE;
    }

    spice_main_clipboard_selection_release(d->main, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD);
    clipboardOwner = CB_OWNER_NONE;

    return FALSE;
}

int SpiceGlibGlue_ReleaseGuestClipboard()
{
    SPICE_DEBUG("CB: ReleaseGuestClipboard");
    if (!enableClipboardToGuest) {
        SPICE_DEBUG("CB: enableClipboardToGuest set to false. Doing nothing.");
        return 0;
    }

    g_idle_add(release_guest_clipboard, NULL);

    return 0;
}

static gboolean clipboard_get_data(gpointer data)
{
    SpiceDisplay *display;
    SpiceDisplayPrivate *d;

    display = global_display();
    if (global_display() == NULL) {
        return FALSE;
    }

    d = SPICE_DISPLAY_GET_PRIVATE(display);
    if (d->main == NULL) {
        return FALSE;
    }

    spice_main_clipboard_selection_request(d->main, 
        VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD,  
        VD_AGENT_CLIPBOARD_UTF8_TEXT);

    return FALSE;
}

int SpiceGlibGlue_ClipboardGetData()
{
    SPICE_DEBUG("CB: ClipboardGetData");

    if (clipboardOwner != CB_OWNER_GUEST) {
        SPICE_DEBUG("CB: Guest has not grabbed CB, returning false");
        return 0;
    }

    g_idle_add(clipboard_get_data, NULL);
    
    return 1;
}

int SpiceGlibGlue_ClipboardDataAvailable()
{
    SPICE_DEBUG("CB: ClipboardDataAvailable");

    g_mutex_lock (&data_mutex);
    if (pendingGuestData == 1) {
        pendingGuestData = 0;
        g_mutex_unlock (&data_mutex);
        return 1;
    }
    g_mutex_unlock (&data_mutex);
    return 0;
}

gboolean clipboard_requestFromGuest(SpiceMainChannel *main, guint selection,
                                  guint type, gpointer user_data)
{                                 
    SPICE_DEBUG("CB: clipboard_requestFromGuest()");
    if (!enableClipboardToGuest) {
        SPICE_DEBUG("CB: enableClipboardToGuest set to false. Doing nothing.");
        return TRUE;
    }

    SpiceDisplay *display;
    SpiceDisplayPrivate *d;
    
    if (clipboardOwner != CB_OWNER_HOST) {
        SPICE_DEBUG("We do NOT have clipboard grabbed, so we won't send it.");
        return FALSE;
    }
    
    display = global_display();
    if (global_display() == NULL) {
        return FALSE;
    }

    d = SPICE_DISPLAY_GET_PRIVATE(display);
    if (d->main == NULL) {
        return FALSE;
    }

    //TODO check values as spice-gtk-session.
    gchar *data = (gchar *) hostClipboard;
    SPICE_DEBUG("CB: hostClipboard %p\n", (void *) hostClipboard);

    if (data == NULL ) {
        SPICE_DEBUG("CB: No supported Clipboard format available\n");
        return FALSE;
    }
        
    /* Transform format (line ending, etc) before sending */ 
    gint len = 0;
    gpointer conv = NULL;
    
    /* Our clipboard is not GTK but windows. So we positively have CRLF as line break.
       But it is our (client program) responsability to give the guest the format it wants.
    */
    if (spice_main_agent_test_capability(d->main, VD_AGENT_CAP_GUEST_LINEEND_CRLF)) {
        SPICE_DEBUG("CB: Host to Guest, changing line ending\n");
        len = strnlen(data, CB_SIZE);
        conv = spice_unix2dos((gchar*)data, len);
        if (conv == NULL) {
            return FALSE;
        }
        SPICE_DEBUG("CB: Host to Guest, changing line ending: OK\n");
        len = strnlen(conv, CB_SIZE);
    } else {
        len = strnlen((const char *)data, CB_SIZE);
    }

    spice_main_clipboard_selection_notify(d->main, 
            VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, 
            VD_AGENT_CLIPBOARD_UTF8_TEXT,
            conv ? conv : data, len);
    g_free(conv);
    return FALSE;
}

void clipboard_got_from_guest(SpiceMainChannel *main, guint selection,
                                     guint type, const guchar *data, guint size,
                                     gpointer user_data)
{
    SPICE_DEBUG("CB: clipboard_got_from_guest type : %d ", type);
    if (!enableClipboardToClient) {
        SPICE_DEBUG("CB: enableClipboardToClient set to false. Doing nothing.");
        return;
    }

    if (type == VD_AGENT_CLIPBOARD_NONE) {
        SPICE_DEBUG("CB: No data. Received type VD_AGENT_CLIPBOARD_NONE : size %d", size);
        return;
    }
    
    if (type != VD_AGENT_CLIPBOARD_UTF8_TEXT) {
        g_warning("CB: Ignoring clipboard of unexpected type %d from guest", type);
        return;
    }

    push_clipboard_data (data, size);

    if (clientClipboardCallback != NULL) {
        clientClipboardCallback((char *) guestClipboard);
    }
}

gboolean clipboard_grabByGuest(SpiceMainChannel *main, guint selection,
                               guint32* types, guint32 num_types,
                               gpointer user_data) {

    gint i;
    
    SPICE_DEBUG("CB: clipboard_grabByGuest(sel %d)", selection);
    if (!enableClipboardToClient) {
        SPICE_DEBUG("CB: enableClipboardToClient set to false. Doing nothing.");
        return TRUE;
    }
        
    if (selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        g_warning("CB: discarded clipboard request of unsupported selection %d",selection);
        return FALSE;    
    }
    
    for (i = 0; i < num_types; i++) {
        SPICE_DEBUG("CB: checking type(%d)",types[i]);
        if (types[i] == VD_AGENT_CLIPBOARD_UTF8_TEXT){
        
            SPICE_DEBUG("CB: IT IS UTF8");
            spice_main_channel_clipboard_selection_request(main, selection, types[i]);
            clipboardOwner = CB_OWNER_GUEST;
        }
    }

    return TRUE;
}

gboolean clipboard_releaseByGuest(SpiceMainChannel *main, guint selection,
    guint32* types, guint32 num_types,
    gpointer user_data) {

    SPICE_DEBUG("CB: clipboard_releaseByGuest(sel %d) not implemented in this platform", selection);

    return TRUE;
}

gboolean SpiceGlibGlue_InitClipboard(
        int16_t enableClipboardToGuestP, int16_t enableClipboardToClientP,
        uint32_t *guestClipboardP, uint32_t *hostClipboardP,
        void (*clientClipboardCallbackP)(char *))
{
    SPICE_DEBUG("CB SpiceGlibGlue_InitClipboard (%d, %d)", 
            enableClipboardToGuestP, enableClipboardToClientP);
    enableClipboardToGuest  = enableClipboardToGuestP;
    enableClipboardToClient = enableClipboardToClientP;

    guestClipboard = guestClipboardP;
    hostClipboard = hostClipboardP;
    clientClipboardCallback = clientClipboardCallbackP;
    SPICE_DEBUG("CB: guestClipboard %p\n", (void *) guestClipboard);
    SPICE_DEBUG("CB: hostClipboard %p\n", (void *) hostClipboard);
    
    return FALSE;    
}

void spice_clipboard_selection_grab(SpiceMainChannel *channel, char *text, int size) {
    int writeSize = MIN(size, CB_SIZE - 1);
    SPICE_DEBUG("CB: spice_clipboard_selection_grab writeSize: %d", writeSize);
    snprintf((char *) hostClipboard, writeSize + 1, "%s", text);
    guint32 clipboard_types[] = { VD_AGENT_CLIPBOARD_UTF8_TEXT };
    spice_main_channel_clipboard_selection_grab(
            channel,
            VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD,
            clipboard_types,
            1
    );
}

struct client_cut_text {
    char *contents;
    int size;
};

static gboolean client_cut_text(gpointer data)
{
    struct client_cut_text *cut = data;
    SpiceDisplay *display = global_display();
    SpiceDisplayPrivate *d = display != NULL ? SPICE_DISPLAY_GET_PRIVATE(display) : NULL;

    if (d != NULL && d->main != NULL) {
        spice_clipboard_selection_grab(d->main, cut->contents, cut->size);
    }
    g_free(cut->contents);
    g_free(cut);
    return G_SOURCE_REMOVE;
}

gboolean SpiceGlibGlue_ClientCutText(char *hostClipboardContents, int size) {
    if (hostClipboard == NULL || hostClipboardContents == NULL || size <= 0) {
        return FALSE;
    }

    struct client_cut_text *cut = g_new0(struct client_cut_text, 1);
    cut->contents = g_malloc0(size + 1);
    memcpy(cut->contents, hostClipboardContents, size);
    cut->size = size;
    g_idle_add(client_cut_text, cut);
    return TRUE;
}
