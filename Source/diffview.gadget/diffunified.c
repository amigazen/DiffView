/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * diffunified.c - unified diff input for CreateDiffObject().
 *
 * Builds aligned left/right DiffFile output from a unified diff buffer plus
 * either an original (old) or modified (new) memory file, per diffview.doc.
 */

#include <string.h>
#include <ctype.h>

#include "classbase.h"
#include "diffunified.h"
#include "difffile.h"
#include "diffline.h"
#include "Debug.h"

#define DU_GROW 64

typedef struct DuLineArray
{
    STRPTR *da_Lines;
    ULONG   da_Count;
    ULONG   da_Cap;
} DuLineArray;

static void du_Report(DIFFOBJPROGCB progCB, APTR progData, ULONG percent)
{
    if (progCB)
        progCB(percent, progData);
}

static BOOL du_ArrayAdd(struct DiffViewClassBase *cb, DuLineArray *arr,
                        STRPTR text, ULONG len)
{
    STRPTR *newLines;
    STRPTR copy;
    ULONG newCap;

    if (!arr)
        return FALSE;

    if (arr->da_Count >= arr->da_Cap)
    {
        newCap = arr->da_Cap + DU_GROW;
        newLines = (STRPTR *) AllocMem(newCap * sizeof(STRPTR), MEMF_CLEAR);
        if (!newLines)
            return FALSE;
        if (arr->da_Lines)
        {
            memcpy(newLines, arr->da_Lines, arr->da_Count * sizeof(STRPTR));
            FreeMem(arr->da_Lines, arr->da_Cap * sizeof(STRPTR));
        }
        arr->da_Lines = newLines;
        arr->da_Cap = newCap;
    }

    if (!len)
    {
        arr->da_Lines[arr->da_Count++] = NULL;
        return TRUE;
    }

    copy = (STRPTR) AllocMem(len + 1, MEMF_CLEAR);
    if (!copy)
        return FALSE;
    memcpy(copy, text, len);
    copy[len] = '\0';
    arr->da_Lines[arr->da_Count++] = copy;
    return TRUE;
}

static void du_ArrayFree(DuLineArray *arr)
{
    ULONG i;

    if (!arr)
        return;

    if (arr->da_Lines)
    {
        for (i = 0; i < arr->da_Count; i++)
        {
            if (arr->da_Lines[i])
                FreeMem(arr->da_Lines[i], strlen(arr->da_Lines[i]) + 1);
        }
        FreeMem(arr->da_Lines, arr->da_Cap * sizeof(STRPTR));
    }
    arr->da_Lines = NULL;
    arr->da_Count = 0;
    arr->da_Cap = 0;
}

static BOOL du_ParseFileLines(struct DiffViewClassBase *cb, DuLineArray *arr,
                              STRPTR buf, ULONG len)
{
    ULONG i;
    ULONG lineStart;

    if (!arr)
        return FALSE;

    if (!buf || !len)
        return TRUE;

    lineStart = 0;
    for (i = 0; i < len; i++)
    {
        if (buf[i] == '\n')
        {
            if (!du_ArrayAdd(cb, arr, buf + lineStart, i - lineStart))
                return FALSE;
            lineStart = i + 1;
        }
    }

    if (lineStart < len)
    {
        if (!du_ArrayAdd(cb, arr, buf + lineStart, len - lineStart))
            return FALSE;
    }

    return TRUE;
}

static BOOL du_AddOutputLine(struct DiffViewClassBase *cb, DiffFile *file,
                             STRPTR text, UBYTE state)
{
    DiffLine *line;
    STRPTR copy;

    if (!file)
        return FALSE;

    if (!text)
        return DiffFile_AddEmptyLine(cb, file);

    copy = (STRPTR) AllocMem(strlen(text) + 1, MEMF_CLEAR);
    if (!copy)
        return FALSE;
    strcpy(copy, text);

    line = (DiffLine *) AllocMem(sizeof(DiffLine), MEMF_CLEAR);
    if (!line)
    {
        FreeMem(copy, strlen(text) + 1);
        return FALSE;
    }

    DiffLine_Init(line, copy, 0);
    DiffLine_SetState(line, state);
    if (!DiffFile_AddLine(cb, file, line))
    {
        DiffLine_Free(cb, line);
        FreeMem(line, sizeof(DiffLine));
        return FALSE;
    }
    return TRUE;
}

static BOOL du_AddDiffPos(struct DiffViewClassBase *cb, DiffObject *obj,
                          ULONG zeroBasedIdx)
{
    ULONG *newPos;
    ULONG newCap;

    if (!obj)
        return FALSE;

    if (obj->do_NumDiffPositions >= obj->do_DiffPosCap)
    {
        newCap = obj->do_DiffPosCap + DU_GROW;
        newPos = (ULONG *) AllocMem(newCap * sizeof(ULONG), MEMF_CLEAR);
        if (!newPos)
            return FALSE;
        if (obj->do_DiffPositions)
        {
            memcpy(newPos, obj->do_DiffPositions,
                   obj->do_NumDiffPositions * sizeof(ULONG));
            FreeMem(obj->do_DiffPositions,
                    obj->do_DiffPosCap * sizeof(ULONG));
        }
        obj->do_DiffPositions = newPos;
        obj->do_DiffPosCap = newCap;
    }

    obj->do_DiffPositions[obj->do_NumDiffPositions++] = zeroBasedIdx + 1;
    return TRUE;
}

static BOOL du_EmitNormal(struct DiffViewClassBase *cb, DiffObject *obj,
                          STRPTR text)
{
    if (!du_AddOutputLine(cb, &obj->do_Left, text, DLS_NORMAL))
        return FALSE;
    if (!du_AddOutputLine(cb, &obj->do_Right, text, DLS_NORMAL))
        return FALSE;
    return TRUE;
}

static BOOL du_EmitChange(struct DiffViewClassBase *cb, DiffObject *obj,
                          STRPTR leftText, UBYTE leftState,
                          STRPTR rightText, UBYTE rightState,
                          UBYTE markDiff)
{
    ULONG idx;

    if (markDiff)
    {
        idx = DiffFile_NumLines(&obj->do_Left);
        if (!du_AddDiffPos(cb, obj, idx))
            return FALSE;
    }

    if (!du_AddOutputLine(cb, &obj->do_Left, leftText, leftState))
        return FALSE;
    if (!du_AddOutputLine(cb, &obj->do_Right, rightText, rightState))
        return FALSE;
    return TRUE;
}

static BOOL du_ParseSpan(STRPTR *p, LONG *value, LONG *count)
{
    STRPTR s;
    LONG v;
    LONG c;

    s = *p;
    v = 0;
    if (*s == '-')
        return FALSE;
    while (*s && isdigit((unsigned char) *s))
    {
        v = v * 10 + (*s - '0');
        s++;
    }
    if (v < 1)
        v = 1;
    c = 1;
    if (*s == ',')
    {
        s++;
        c = 0;
        while (*s && isdigit((unsigned char) *s))
        {
            c = c * 10 + (*s - '0');
            s++;
        }
        if (c < 0)
            c = 0;
    }
    *value = v;
    *count = c;
    *p = s;
    return TRUE;
}

static BOOL du_ParseHunkHeader(STRPTR line, ULONG lineLen,
                               LONG *oldStart, LONG *oldCount,
                               LONG *newStart, LONG *newCount)
{
    ULONG i;
    STRPTR s;

    if (!line || lineLen < 4)
        return FALSE;
    if (line[0] != '@' || line[1] != '@')
        return FALSE;

    i = 2;
    while (i < lineLen && line[i] == ' ')
        i++;
    if (i >= lineLen || line[i] != '-')
        return FALSE;

    s = line + i + 1;
    if (!du_ParseSpan(&s, oldStart, oldCount))
        return FALSE;
    while (*s == ' ')
        s++;
    if (*s != '+')
        return FALSE;
    s++;
    if (!du_ParseSpan(&s, newStart, newCount))
        return FALSE;
    return TRUE;
}

static STRPTR du_LineText(STRPTR line, ULONG lineLen, UBYTE kind,
                          ULONG *textLen)
{
    ULONG start;

    if (!line || lineLen < 1)
    {
        *textLen = 0;
        return NULL;
    }

    if (line[0] != kind)
    {
        *textLen = 0;
        return NULL;
    }

    start = 1;
    if (lineLen > 1 && line[1] == ' ')
        start = 2;
    else if (lineLen > 1 && line[1] == '\r')
        start = 1;

    if (start >= lineLen)
    {
        *textLen = 0;
        return (STRPTR) "";
    }

    *textLen = lineLen - start;
    if (*textLen > 0 && line[start + *textLen - 1] == '\r')
        (*textLen)--;

    return line + start;
}

static BOOL du_StripsEqual(STRPTR a, STRPTR b)
{
    STRPTR pa;
    STRPTR pb;
    UBYTE ca;
    UBYTE ch;

    pa = a;
    pb = b;
    if (!pa)
        pa = (STRPTR) "";
    if (!pb)
        pb = (STRPTR) "";

    while (*pa == ' ' || *pa == '\t')
        pa++;
    while (*pb == ' ' || *pb == '\t')
        pb++;

    while (*pa || *pb)
    {
        ca = (UBYTE) *pa;
        ch = (UBYTE) *pb;
        if (ca == ' ' || ca == '\t')
        {
            pa++;
            continue;
        }
        if (ch == ' ' || ch == '\t')
        {
            pb++;
            continue;
        }
        if (ca != ch)
            return FALSE;
        pa++;
        pb++;
    }
    return TRUE;
}

static void du_MarkWsOnly(DiffFile *left, DiffFile *right)
{
    ULONG i;
    ULONG n;
    DiffLine *la;
    DiffLine *rb;

    n = DiffFile_NumLines(left);
    if (DiffFile_NumLines(right) > n)
        n = DiffFile_NumLines(right);

    for (i = 0; i < n; i++)
    {
        la = DiffFile_Line(left, i);
        rb = DiffFile_Line(right, i);
        if (!la || !rb)
            continue;
        if (DiffLine_State(la) == DLS_CHANGED &&
            DiffLine_State(rb) == DLS_CHANGED &&
            du_StripsEqual(DiffLine_Text(la), DiffLine_Text(rb)))
        {
            DiffLine_SetState(la, DLS_WS_ONLY);
            DiffLine_SetState(rb, DLS_WS_ONLY);
        }
    }
}

static BOOL du_BuildFromOld(struct DiffViewClassBase *cb, DiffObject *obj,
                            DuLineArray *oldLines, STRPTR diffBuf, ULONG diffLen,
                            DIFFOBJPROGCB progCB, APTR progData)
{
    ULONG pos;
    ULONG lineStart;
    ULONG i;
    ULONG lastPercent;
    STRPTR line;
    ULONG lineLen;
    LONG hunkOldStart;
    LONG hunkOldCount;
    LONG hunkNewStart;
    LONG hunkNewCount;
    ULONG oldIdx;
    ULONG inHunk;
    UBYTE blockStarted;
    STRPTR text;
    ULONG textLen;

    oldIdx = 0;
    lastPercent = 0;
    du_Report(progCB, progData, 0);

    pos = 0;
    while (pos < diffLen)
    {
        lineStart = pos;
        while (pos < diffLen && diffBuf[pos] != '\n')
            pos++;
        lineLen = pos - lineStart;
        if (pos < diffLen)
            pos++;

        if (lineLen >= 4 && diffBuf[lineStart] == '@' && diffBuf[lineStart + 1] == '@')
        {
            if (!du_ParseHunkHeader(diffBuf + lineStart, lineLen,
                                    &hunkOldStart, &hunkOldCount,
                                    &hunkNewStart, &hunkNewCount))
                return FALSE;

            while ((LONG) oldIdx < hunkOldStart - 1 &&
                   oldIdx < oldLines->da_Count)
            {
                if (!du_EmitNormal(cb, obj, oldLines->da_Lines[oldIdx]))
                    return FALSE;
                oldIdx++;
            }

            inHunk = 1;
            blockStarted = 0;
            while (inHunk && pos < diffLen)
            {
                lineStart = pos;
                while (pos < diffLen && diffBuf[pos] != '\n')
                    pos++;
                lineLen = pos - lineStart;
                if (pos < diffLen)
                    pos++;

                line = diffBuf + lineStart;
                if (lineLen >= 4 && line[0] == '@' && line[1] == '@')
                {
                    pos = lineStart;
                    break;
                }

                if (lineLen < 1)
                    continue;

                if (line[0] == ' ')
                {
                    text = du_LineText(line, lineLen, ' ', &textLen);
                    if (!du_EmitNormal(cb, obj, text))
                        return FALSE;
                    if (oldIdx < oldLines->da_Count)
                        oldIdx++;
                    blockStarted = 0;
                }
                else if (line[0] == '-')
                {
                    text = du_LineText(line, lineLen, '-', &textLen);
                    if (!blockStarted)
                        blockStarted = 1;
                    if (!du_EmitChange(cb, obj, text, DLS_DELETED, NULL,
                                       DLS_NORMAL, blockStarted))
                        return FALSE;
                    blockStarted = 0;
                    if (oldIdx < oldLines->da_Count)
                        oldIdx++;
                }
                else if (line[0] == '+')
                {
                    text = du_LineText(line, lineLen, '+', &textLen);
                    if (!blockStarted)
                        blockStarted = 1;
                    if (!du_EmitChange(cb, obj, NULL, DLS_NORMAL, text,
                                       DLS_ADDED, blockStarted))
                        return FALSE;
                    blockStarted = 0;
                }
            }

            (void) hunkNewStart;
            (void) hunkNewCount;
        }

        i = (pos * 100) / diffLen;
        if (i > lastPercent)
        {
            while (lastPercent < i && lastPercent < 100)
            {
                lastPercent++;
                du_Report(progCB, progData, lastPercent);
            }
        }
    }

    while (oldIdx < oldLines->da_Count)
    {
        if (!du_EmitNormal(cb, obj, oldLines->da_Lines[oldIdx]))
            return FALSE;
        oldIdx++;
    }

    du_Report(progCB, progData, 100);
    return TRUE;
}

static BOOL du_BuildFromNew(struct DiffViewClassBase *cb, DiffObject *obj,
                            DuLineArray *newLines, STRPTR diffBuf, ULONG diffLen,
                            DIFFOBJPROGCB progCB, APTR progData)
{
    ULONG pos;
    ULONG lineStart;
    ULONG i;
    ULONG lastPercent;
    STRPTR line;
    ULONG lineLen;
    LONG hunkOldStart;
    LONG hunkOldCount;
    LONG hunkNewStart;
    LONG hunkNewCount;
    ULONG newIdx;
    UBYTE blockStarted;
    STRPTR text;
    ULONG textLen;

    newIdx = 0;
    lastPercent = 0;
    du_Report(progCB, progData, 0);

    pos = 0;
    while (pos < diffLen)
    {
        lineStart = pos;
        while (pos < diffLen && diffBuf[pos] != '\n')
            pos++;
        lineLen = pos - lineStart;
        if (pos < diffLen)
            pos++;

        if (lineLen >= 4 && diffBuf[lineStart] == '@' && diffBuf[lineStart + 1] == '@')
        {
            if (!du_ParseHunkHeader(diffBuf + lineStart, lineLen,
                                    &hunkOldStart, &hunkOldCount,
                                    &hunkNewStart, &hunkNewCount))
                return FALSE;

            while ((LONG) newIdx < hunkNewStart - 1 &&
                   newIdx < newLines->da_Count)
            {
                if (!du_EmitNormal(cb, obj, newLines->da_Lines[newIdx]))
                    return FALSE;
                newIdx++;
            }

            blockStarted = 0;
            while (pos < diffLen)
            {
                lineStart = pos;
                while (pos < diffLen && diffBuf[pos] != '\n')
                    pos++;
                lineLen = pos - lineStart;
                if (pos < diffLen)
                    pos++;

                line = diffBuf + lineStart;
                if (lineLen >= 4 && line[0] == '@' && line[1] == '@')
                {
                    pos = lineStart;
                    break;
                }

                if (lineLen < 1)
                    continue;

                if (line[0] == ' ')
                {
                    text = du_LineText(line, lineLen, ' ', &textLen);
                    if (!du_EmitNormal(cb, obj, text))
                        return FALSE;
                    if (newIdx < newLines->da_Count)
                        newIdx++;
                    blockStarted = 0;
                }
                else if (line[0] == '+')
                {
                    text = du_LineText(line, lineLen, '+', &textLen);
                    if (!blockStarted)
                        blockStarted = 1;
                    if (!du_EmitChange(cb, obj, NULL, DLS_NORMAL, text,
                                       DLS_ADDED, blockStarted))
                        return FALSE;
                    blockStarted = 0;
                    if (newIdx < newLines->da_Count)
                        newIdx++;
                }
                else if (line[0] == '-')
                {
                    text = du_LineText(line, lineLen, '-', &textLen);
                    if (!blockStarted)
                        blockStarted = 1;
                    if (!du_EmitChange(cb, obj, text, DLS_DELETED, NULL,
                                       DLS_NORMAL, blockStarted))
                        return FALSE;
                    blockStarted = 0;
                }
            }

            (void) hunkOldStart;
            (void) hunkOldCount;
        }

        i = (pos * 100) / diffLen;
        if (i > lastPercent)
        {
            while (lastPercent < i && lastPercent < 100)
            {
                lastPercent++;
                du_Report(progCB, progData, lastPercent);
            }
        }
    }

    while (newIdx < newLines->da_Count)
    {
        if (!du_EmitNormal(cb, obj, newLines->da_Lines[newIdx]))
            return FALSE;
        newIdx++;
    }

    du_Report(progCB, progData, 100);
    return TRUE;
}

BOOL DiffUnified_Build(struct DiffViewClassBase *cb, DiffObject *obj,
                      STRPTR oldBuf, ULONG oldLen,
                      STRPTR newBuf, ULONG newLen,
                      STRPTR diffBuf, ULONG diffLen,
                      UBYTE wsUnimportant,
                      DIFFOBJPROGCB progCB, APTR progData)
{
    DuLineArray base;
    BOOL ok;

    if (!cb || !obj || !diffBuf || !diffLen)
        return FALSE;

    base.da_Lines = NULL;
    base.da_Count = 0;
    base.da_Cap = 0;

    ok = FALSE;
    if (oldBuf && oldLen)
    {
        if (!du_ParseFileLines(cb, &base, oldBuf, oldLen))
            goto done;
        ok = du_BuildFromOld(cb, obj, &base, diffBuf, diffLen,
                             progCB, progData);
    }
    else if (newBuf && newLen)
    {
        if (!du_ParseFileLines(cb, &base, newBuf, newLen))
            goto done;
        ok = du_BuildFromNew(cb, obj, &base, diffBuf, diffLen,
                             progCB, progData);
    }

done:
    du_ArrayFree(&base);

    if (!ok)
        return FALSE;

    if (wsUnimportant)
        du_MarkWsOnly(&obj->do_Left, &obj->do_Right);

    return TRUE;
}
