#include <stdio.h>
#include <string.h>

#include "glib.h"

typedef enum
{
    NEWLINE_TYPE_LF,
    NEWLINE_TYPE_CR_LF
} NewlineType;

static gssize get_line(const gchar *str, gsize len,
                       NewlineType type, gsize *nl_len)
{
    const gchar *p, *endl;
    gsize nl = 0;

    endl = (type == NEWLINE_TYPE_CR_LF) ? "\r\n" : "\n";
    p = g_strstr_len(str, len, endl);
    if (p)
    {
        len = p - str;
        nl = strlen(endl);
    }

    *nl_len = nl;
    return len;
}

static gchar *spice_convert_newlines(const gchar *str, gssize len,
                                     NewlineType from,
                                     NewlineType to)
{
    gssize length;
    gsize nl;
    GString *output;
    gint i;

    g_return_val_if_fail(str != NULL, NULL);
    g_return_val_if_fail(len >= -1, NULL);
    /* only 2 supported combinations */
    g_return_val_if_fail((from == NEWLINE_TYPE_LF &&
                          to == NEWLINE_TYPE_CR_LF) ||
                             (from == NEWLINE_TYPE_CR_LF &&
                              to == NEWLINE_TYPE_LF),
                         NULL);

    if (len == -1)
        len = strlen(str);
    /* sometime we get \0 terminated strings, skip that, or it fails
       to utf8 validate line with \0 end */
    else if (len > 0 && str[len - 1] == 0)
        len -= 1;

    /* allocate worst case, if it's small enough, we don't care much,
     * if it's big, malloc will put us in mmap'd region, and we can
     * over allocate.
     */
    output = g_string_sized_new(len * 2 + 1);

    for (i = 0; i < len; i += length + nl)
    {
        length = get_line(str + i, len - i, from, &nl);
        if (length < 0)
            break;

        g_string_append_len(output, str + i, length);

        if (nl)
        {
            /* let's not double \r if it's already in the line */
            if (to == NEWLINE_TYPE_CR_LF &&
                (output->len == 0 || output->str[output->len - 1] != '\r'))
                g_string_append_c(output, '\r');

            g_string_append_c(output, '\n');
        }
    }

    return g_string_free(output, FALSE);
}

G_GNUC_INTERNAL
gchar *spice_dos2unix(const gchar *str, gssize len)
{
    return spice_convert_newlines(str, len,
                                  NEWLINE_TYPE_CR_LF,
                                  NEWLINE_TYPE_LF);
}

G_GNUC_INTERNAL
gchar *spice_unix2dos(const gchar *str, gssize len)
{
    return spice_convert_newlines(str, len,
                                  NEWLINE_TYPE_LF,
                                  NEWLINE_TYPE_CR_LF);
}
