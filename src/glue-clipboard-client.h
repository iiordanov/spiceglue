/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/**
 * Copyright (C) 2023 Iordan Iordanov
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
 * Functions to use clipboard sharing in spice client programs.
 */

#ifdef USE_CLIPBOARD
#ifndef _GLUE_CLIPBOARD_CLIENT_H
#define _GLUE_CLIPBOARD_CLIENT_H

#define CB_SIZE 512 * 1024

gboolean SpiceGlibGlue_InitClipboard(
        int16_t enableClipboardToGuestP, int16_t enableClipboardToClientP,
        uint32_t *guestClipboardP, uint32_t *hostClipboardP,
        void (*clientClipboardCallbackP)(char *));

int SpiceGlibGlue_ClipboardDataAvailable(void);
int SpiceGlibGlue_GrabGuestClipboard(void);
gboolean SpiceGlibGlue_ClientCutText(char *hostClipboardContents, int size);
int SpiceGlibGlue_ReleaseGuestClipboard(void);
int SpiceGlibGlue_ClipboardGetData(void);

#endif /* _GLUE_CLIPBOARD_CLIENT_H */
#endif
