/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * debug.c - optional trace output for diffview.gadget bring-up.
 *
 * PutStr-only tracing through cb->dvb_DOSBase (vector.image vdbg pattern).
 * Avoid Printf here: varargs + romtag/SMALLDATA is fragile during bring-up.
 */

#ifdef DIFFVIEW_DEBUG

#include "classbase.h"
#include "Debug.h"

static void dv_WriteZ(struct DiffViewClassBase *cb, CONST_STRPTR text)
{
    if (DOSBase && text)
        PutStr((STRPTR) text);
}

static void dv_EndLine(struct DiffViewClassBase *cb)
{
    if (DOSBase)
        PutStr((STRPTR) "\n");
}

static void dv_AppendHex(UBYTE *buf, ULONG *pos, ULONG max, ULONG value)
{
    LONG shift;
    ULONG nib;
    ULONG p;

    p = *pos;
    for (shift = 28; shift >= 0; shift -= 4)
    {
        if (p + 1 >= max)
            break;
        nib = (value >> shift) & 0x0FUL;
        if (nib < 10)
            buf[p++] = (UBYTE) ('0' + nib);
        else
            buf[p++] = (UBYTE) ('A' + (nib - 10));
    }
    *pos = p;
}

static void dv_AppendULong(UBYTE *buf, ULONG *pos, ULONG max, ULONG value)
{
    UBYTE tmp[12];
    ULONG count;
    ULONG p;
    ULONG i;

    count = 0;
    if (value == 0)
        tmp[count++] = '0';
    else
    {
        while (value > 0)
        {
            tmp[count++] = (UBYTE) ('0' + (value % 10));
            value /= 10;
        }
    }

    p = *pos;
    for (i = count; i > 0; i--)
    {
        if (p + 1 >= max)
            break;
        buf[p++] = tmp[i - 1];
    }
    *pos = p;
}

void dv_TraceMsg(struct DiffViewClassBase *cb, CONST_STRPTR msg)
{
    if (!DOSBase)
        return;
    dv_WriteZ(cb, (STRPTR) "[diffview] ");
    dv_WriteZ(cb, msg);
    dv_EndLine(cb);
}

void dv_TraceHex(struct DiffViewClassBase *cb, CONST_STRPTR label, APTR ptr)
{
    UBYTE buf[80];
    ULONG pos;
    ULONG i;

    if (!DOSBase)
        return;

    pos = 0;
    buf[pos++] = (UBYTE) '[';
    buf[pos++] = (UBYTE) 'd';
    buf[pos++] = (UBYTE) 'i';
    buf[pos++] = (UBYTE) 'f';
    buf[pos++] = (UBYTE) 'f';
    buf[pos++] = (UBYTE) 'v';
    buf[pos++] = (UBYTE) 'i';
    buf[pos++] = (UBYTE) 'e';
    buf[pos++] = (UBYTE) 'w';
    buf[pos++] = (UBYTE) ']';
    buf[pos++] = (UBYTE) ' ';

    if (label)
    {
        for (i = 0; label[i] != '\0' && pos + 1 < sizeof(buf); i++)
            buf[pos++] = (UBYTE) label[i];
    }

    if (pos + 12 < sizeof(buf))
    {
        buf[pos++] = (UBYTE) ' ';
        buf[pos++] = (UBYTE) '0';
        buf[pos++] = (UBYTE) 'x';
        dv_AppendHex(buf, &pos, (ULONG) sizeof(buf), (ULONG) ptr);
    }

    buf[pos] = '\0';
    dv_WriteZ(cb, (STRPTR) buf);
    dv_EndLine(cb);
}

void dv_TraceULong(struct DiffViewClassBase *cb, CONST_STRPTR label, ULONG value)
{
    UBYTE buf[80];
    ULONG pos;
    ULONG i;

    if (!DOSBase)
        return;

    pos = 0;
    buf[pos++] = (UBYTE) '[';
    buf[pos++] = (UBYTE) 'd';
    buf[pos++] = (UBYTE) 'i';
    buf[pos++] = (UBYTE) 'f';
    buf[pos++] = (UBYTE) 'f';
    buf[pos++] = (UBYTE) 'v';
    buf[pos++] = (UBYTE) 'i';
    buf[pos++] = (UBYTE) 'e';
    buf[pos++] = (UBYTE) 'w';
    buf[pos++] = (UBYTE) ']';
    buf[pos++] = (UBYTE) ' ';

    if (label)
    {
        for (i = 0; label[i] != '\0' && pos + 1 < sizeof(buf); i++)
            buf[pos++] = (UBYTE) label[i];
    }

    if (pos + 14 < sizeof(buf))
    {
        buf[pos++] = (UBYTE) ' ';
        dv_AppendULong(buf, &pos, (ULONG) sizeof(buf), value);
    }

    buf[pos] = '\0';
    dv_WriteZ(cb, (STRPTR) buf);
    dv_EndLine(cb);
}

#endif
