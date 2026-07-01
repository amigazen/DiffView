/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * diffobject.c - CreateDiffObject / FreeDiffObject / GetDiffObjectAttr.
 */

#include <string.h>

#include "classbase.h"
#include "diffobject.h"
#include "diffengine.h"
#include "diffunified.h"
#include "Debug.h"

struct DiffViewClassBase *do_LibBase = NULL;

static const STRPTR do_ErrNoMem = "Out of memory";
static const STRPTR do_ErrNoInput = "No input files specified";
static const STRPTR do_ErrDiffParse = "Unified diff format could not be parsed";

static BOOL do_TagPresent(ULONG tagValue, const struct TagItem *tagList)
{
    const struct TagItem *ti;

    if (!tagList)
        return FALSE;

    for (ti = tagList; ti->ti_Tag != TAG_END; ti++)
    {
        if (ti->ti_Tag == TAG_SKIP)
        {
            if (ti->ti_Data)
                ti = (const struct TagItem *) ti->ti_Data - 1;
        }
        else if (ti->ti_Tag == tagValue)
            return TRUE;
    }
    return FALSE;
}

/* Walk tags locally; do not call utility.library GetTagData from here. */
static ULONG do_GetTagData(ULONG tagValue, ULONG defaultVal,
                           const struct TagItem *tagList)
{
    const struct TagItem *ti;

    if (!tagList)
        return defaultVal;

    for (ti = tagList; ti->ti_Tag != TAG_END; ti++)
    {
        if (ti->ti_Tag == TAG_SKIP)
        {
            if (ti->ti_Data)
                ti = (const struct TagItem *) ti->ti_Data - 1;
        }
        else if (ti->ti_Tag == tagValue)
            return ti->ti_Data;
    }

    return defaultVal;
}

/* Both pointer and length tags must be present or the pair is ignored. */
static BOOL do_BufferPair(const struct TagItem *tags, ULONG tagPtr, ULONG tagLen,
                          STRPTR *buf, ULONG *len)
{
    STRPTR b;
    ULONG l;

    *buf = NULL;
    *len = 0;
    if (!do_TagPresent(tagPtr, tags) || !do_TagPresent(tagLen, tags))
        return FALSE;

    b = (STRPTR) do_GetTagData(tagPtr, (ULONG) NULL, tags);
    l = (ULONG) do_GetTagData(tagLen, 0, tags);
    if (!b || !l)
        return FALSE;

    *buf = b;
    *len = l;
    return TRUE;
}

static void do_SetError(STRPTR *errStr, ULONG *errNum, STRPTR msg, ULONG code)
{
    if (errStr)
        *errStr = msg;
    if (errNum)
        *errNum = code;
}

static BOOL do_AddLineFromSpan(struct DiffViewClassBase *cb, DiffFile *file,
                              STRPTR start, ULONG spanLen, UBYTE skipLeading)
{
    DiffLine *line;
    STRPTR text;

    if (!file)
        return FALSE;

    if (spanLen == 0)
        return DiffFile_AddEmptyLine(cb, file);

    text = (STRPTR) AllocMem(spanLen + 1, MEMF_CLEAR);
    if (!text)
        return FALSE;
    memcpy(text, start, spanLen);
    text[spanLen] = '\0';

    line = (DiffLine *) AllocMem(sizeof(DiffLine), MEMF_CLEAR);
    if (!line)
    {
        FreeMem(text, spanLen + 1);
        return FALSE;
    }

    DiffLine_Init(line, text, skipLeading);
    if (!DiffFile_AddLine(cb, file, line))
    {
        DiffLine_Free(cb, line);
        FreeMem(line, sizeof(DiffLine));
        return FALSE;
    }
    return TRUE;
}

static BOOL do_ParseBuffer(struct DiffViewClassBase *cb, DiffFile *file,
                           STRPTR buf, ULONG len, UBYTE skipLeading)
{
    ULONG i;
    ULONG lineStart;

    if (!file)
        return FALSE;

    if (!buf || !len)
        return TRUE;

    lineStart = 0;
    for (i = 0; i < len; i++)
    {
        if (buf[i] == '\n')
        {
            if (!do_AddLineFromSpan(cb, file, buf + lineStart, i - lineStart,
                                    skipLeading))
                return FALSE;
            lineStart = i + 1;
        }
    }

    if (lineStart < len)
    {
        if (!do_AddLineFromSpan(cb, file, buf + lineStart, len - lineStart,
                                skipLeading))
            return FALSE;
    }

    return TRUE;
}

static BOOL do_StripsEqual(STRPTR a, STRPTR b)
{
    STRPTR pa;
    STRPTR pb;
    UBYTE ca;
    UBYTE ch;

    pa = a;
    pb = b;
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

static void do_MarkWsOnly(DiffFile *left, DiffFile *right)
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
            do_StripsEqual(DiffLine_Text(la), DiffLine_Text(rb)))
        {
            DiffLine_SetState(la, DLS_WS_ONLY);
            DiffLine_SetState(rb, DLS_WS_ONLY);
        }
    }
}

APTR CreateDiffObject(struct DiffViewClassBase *cb, struct TagItem *tags)
{
    DiffObject *obj;
    STRPTR oldBuf;
    ULONG oldLen;
    STRPTR newBuf;
    ULONG newLen;
    STRPTR diffBuf;
    ULONG diffLen;
    STRPTR *errStr;
    ULONG *errNum;
    STRPTR oldName;
    STRPTR newName;
    UBYTE wsUnimportant;
    DIFFOBJPROGCB progCB;
    APTR progData;
    DiffFile leftIn;
    DiffFile rightIn;
    DiffEngine engine;
    ULONG maxLines;
    ULONG tabSize;

    DV_T(cb, "CreateDiffObject enter");
    DV_TH(cb, "cb", cb);
    if (cb)
    {
        DV_TH(cb, "SysBase", cb->dvb_SysBase);
        DV_TH(cb, "UtilityBase", cb->dvb_UtilityBase);
    }

    oldBuf = NULL;
    oldLen = 0;
    newBuf = NULL;
    newLen = 0;
    diffBuf = NULL;
    diffLen = 0;
    errStr = NULL;
    errNum = NULL;
    oldName = NULL;
    newName = NULL;
    wsUnimportant = 1;
    progCB = NULL;
    progData = NULL;
    tabSize = 8;

    if (!cb || !tags)
    {
        do_SetError(errStr, errNum, do_ErrNoInput, DIFFOBJECT_ERROR_NOINPUT);
        return NULL;
    }

    oldBuf = NULL;
    oldLen = 0;
    newBuf = NULL;
    newLen = 0;
    diffBuf = NULL;
    diffLen = 0;
    errStr = NULL;
    errNum = NULL;
    oldName = NULL;
    newName = NULL;
    wsUnimportant = 1;
    progCB = NULL;
    progData = NULL;
    tabSize = 8;

    if (!cb || !tags)
    {
        do_SetError(errStr, errNum, do_ErrNoInput, DIFFOBJECT_ERROR_NOINPUT);
        return NULL;
    }

    (void) do_BufferPair(tags, DIFFOBJECT_OldFile, DIFFOBJECT_OldFileLen,
                         &oldBuf, &oldLen);
    (void) do_BufferPair(tags, DIFFOBJECT_NewFile, DIFFOBJECT_NewFileLen,
                         &newBuf, &newLen);
    (void) do_BufferPair(tags, DIFFOBJECT_Diffs, DIFFOBJECT_DiffsLen,
                         &diffBuf, &diffLen);
    errStr = (STRPTR *) do_GetTagData(DIFFOBJECT_ErrorStr, (ULONG) NULL, tags);
    errNum = (ULONG *) do_GetTagData(DIFFOBJECT_ErrorNum, (ULONG) NULL, tags);
    oldName = (STRPTR) do_GetTagData(DIFFOBJECT_OldFileName, (ULONG) NULL, tags);
    newName = (STRPTR) do_GetTagData(DIFFOBJECT_NewFileName, (ULONG) NULL, tags);
    wsUnimportant = (UBYTE) do_GetTagData(DIFFOBJECT_WsUnimportant, 1, tags);
    progCB = (DIFFOBJPROGCB) do_GetTagData(DIFFOBJECT_ProgressCB, (ULONG) NULL, tags);
    progData = (APTR) do_GetTagData(DIFFOBJECT_ProgCBData, (ULONG) NULL, tags);

    DV_TU(cb, "oldLen", oldLen);
    DV_TU(cb, "newLen", newLen);
    DV_TU(cb, "diffLen", diffLen);
    DV_TH(cb, "oldBuf", oldBuf);
    DV_TH(cb, "newBuf", newBuf);

    if (diffBuf && diffLen)
    {
        if (oldBuf && oldLen && newBuf && newLen)
        {
            do_SetError(errStr, errNum, do_ErrNoInput, DIFFOBJECT_ERROR_NOINPUT);
            return NULL;
        }
        if (!oldBuf && !newBuf)
        {
            do_SetError(errStr, errNum, do_ErrNoInput, DIFFOBJECT_ERROR_NOINPUT);
            return NULL;
        }
    }
    else if (!oldBuf && !newBuf)
    {
        do_SetError(errStr, errNum, do_ErrNoInput, DIFFOBJECT_ERROR_NOINPUT);
        return NULL;
    }

    obj = (DiffObject *) AllocMem(sizeof(DiffObject), MEMF_CLEAR);
    if (!obj)
    {
        do_SetError(errStr, errNum, do_ErrNoMem, DIFFOBJECT_ERROR_NOMEM);
        return NULL;
    }

    DiffFile_Init(&obj->do_Left);
    DiffFile_Init(&obj->do_Right);
    obj->do_TabSize = tabSize;

    if (oldName)
    {
        obj->do_OldFileName = (STRPTR) AllocMem(strlen(oldName) + 1, 0);
        if (obj->do_OldFileName)
            strcpy(obj->do_OldFileName, oldName);
    }
    if (newName)
    {
        obj->do_NewFileName = (STRPTR) AllocMem(strlen(newName) + 1, 0);
        if (obj->do_NewFileName)
            strcpy(obj->do_NewFileName, newName);
    }

    DiffFile_Init(&leftIn);
    DiffFile_Init(&rightIn);

    if (diffBuf && diffLen)
    {
        DV_T(cb, "parse unified diff...");
        if (!DiffUnified_Build(cb, obj, oldBuf, oldLen, newBuf, newLen,
                               diffBuf, diffLen, wsUnimportant,
                               progCB, progData))
        {
            do_SetError(errStr, errNum, do_ErrDiffParse, DIFFOBJECT_ERROR_DIFFPARSE);
            FreeDiffObject(cb, obj);
            return NULL;
        }
    }
    else
    {
        DV_T(cb, "parse buffers...");
        if (!do_ParseBuffer(cb, &leftIn, oldBuf, oldLen, 0) ||
            !do_ParseBuffer(cb, &rightIn, newBuf, newLen, 0))
        {
            do_SetError(errStr, errNum, do_ErrNoMem, DIFFOBJECT_ERROR_NOMEM);
            DiffFile_Free(cb, &leftIn);
            DiffFile_Free(cb, &rightIn);
            FreeDiffObject(cb, obj);
            return NULL;
        }

        maxLines = DiffFile_NumLines(&leftIn);
        if (DiffFile_NumLines(&rightIn) > maxLines)
            maxLines = DiffFile_NumLines(&rightIn);

        DV_TU(cb, "left lines", DiffFile_NumLines(&leftIn));
        DV_TU(cb, "right lines", DiffFile_NumLines(&rightIn));

        if (!DiffFile_CollectLineNumbers(cb, &leftIn, maxLines) ||
            !DiffFile_CollectLineNumbers(cb, &rightIn, maxLines))
        {
            do_SetError(errStr, errNum, do_ErrNoMem, DIFFOBJECT_ERROR_NOMEM);
            DiffFile_Free(cb, &leftIn);
            DiffFile_Free(cb, &rightIn);
            FreeDiffObject(cb, obj);
            return NULL;
        }

        DiffEngine_Init(&engine);
        DV_T(cb, "DiffEngine_Compare...");
        if (!DiffEngine_Compare(cb, &engine, &leftIn, &rightIn,
                                &obj->do_Left, &obj->do_Right,
                                progCB, progData))
        {
            if (engine.de_Cancelled)
                do_SetError(errStr, errNum, NULL, DIFFOBJECT_ERROR_CANCELLED);
            else
                do_SetError(errStr, errNum, do_ErrNoMem, DIFFOBJECT_ERROR_NOMEM);
            DiffEngine_Free(cb, &engine);
            DiffFile_Free(cb, &leftIn);
            DiffFile_Free(cb, &rightIn);
            FreeDiffObject(cb, obj);
            return NULL;
        }

        if (wsUnimportant)
            do_MarkWsOnly(&obj->do_Left, &obj->do_Right);

        obj->do_DiffPositions = engine.de_DiffPositions;
        obj->do_NumDiffPositions = engine.de_NumDiffPositions;
        obj->do_DiffPosCap = engine.de_DiffCap;
        engine.de_DiffPositions = NULL;

        DiffEngine_Free(cb, &engine);
        DiffFile_Free(cb, &leftIn);
        DiffFile_Free(cb, &rightIn);
    }

    if (!DiffFile_CollectLineNumbers(cb, &obj->do_Left,
                                     DiffFile_NumLines(&obj->do_Left)) ||
        !DiffFile_CollectLineNumbers(cb, &obj->do_Right,
                                     DiffFile_NumLines(&obj->do_Right)))
    {
        do_SetError(errStr, errNum, do_ErrNoMem, DIFFOBJECT_ERROR_NOMEM);
        FreeDiffObject(cb, obj);
        return NULL;
    }

    obj->do_MaxLineLen = DiffFile_MaxRenderWidth(&obj->do_Left, tabSize);
    {
        ULONG rw;

        rw = DiffFile_MaxRenderWidth(&obj->do_Right, tabSize);
        if (rw > obj->do_MaxLineLen)
            obj->do_MaxLineLen = rw;
    }

    do_SetError(errStr, errNum, NULL, DIFFOBJECT_ERROR_NONE);
    DV_T(cb, "CreateDiffObject ok");
    return (APTR) obj;
}

void FreeDiffObject(struct DiffViewClassBase *cb, APTR DiffObj)
{
    DiffObject *obj;
    ULONG freeSize;

    obj = (DiffObject *) DiffObj;
    if (!obj || !cb)
        return;

    DiffFile_Free(cb, &obj->do_Left);
    DiffFile_Free(cb, &obj->do_Right);
    if (obj->do_DiffPositions)
    {
        freeSize = obj->do_DiffPosCap;
        if (freeSize < obj->do_NumDiffPositions)
            freeSize = obj->do_NumDiffPositions;
        if (freeSize < 1)
            freeSize = obj->do_NumDiffPositions;
        FreeMem(obj->do_DiffPositions, freeSize * sizeof(ULONG));
    }
    if (obj->do_OldFileName)
        FreeMem(obj->do_OldFileName, strlen(obj->do_OldFileName) + 1);
    if (obj->do_NewFileName)
        FreeMem(obj->do_NewFileName, strlen(obj->do_NewFileName) + 1);
    FreeMem(obj, sizeof(DiffObject));
}

BOOL GetDiffObjectAttr(struct DiffViewClassBase *cb, APTR DiffObj,
                       ULONG Attr, APTR Storage)
{
    DiffObject *obj;
    struct DiffPosition *pos;
    ULONG *copy;

    (void) cb;
    obj = (DiffObject *) DiffObj;
    if (!obj || !Storage)
        return FALSE;

    switch (Attr)
    {
        case DIFFOBJECT_DiffPositions:
            pos = (struct DiffPosition *) Storage;
            pos->dp_Positions = NULL;
            pos->dp_Count = 0;
            if (!obj->do_DiffPositions || obj->do_NumDiffPositions < 1)
                return TRUE;

            copy = (ULONG *) AllocVec(obj->do_NumDiffPositions * sizeof(ULONG),
                                      MEMF_CLEAR);
            if (!copy)
                return FALSE;
            memcpy(copy, obj->do_DiffPositions,
                   obj->do_NumDiffPositions * sizeof(ULONG));
            pos->dp_Positions = copy;
            pos->dp_Count = obj->do_NumDiffPositions;
            return TRUE;

        default:
            break;
    }
    return FALSE;
}

APTR CreateDiffObjectTags(Tag tag1, ...)
{
    if (!do_LibBase)
        return NULL;
    return CreateDiffObject(do_LibBase, (struct TagItem *) &tag1);
}

DiffFile *DiffObject_Left(DiffObject *obj)
{
    if (!obj)
        return NULL;
    return &obj->do_Left;
}

DiffFile *DiffObject_Right(DiffObject *obj)
{
    if (!obj)
        return NULL;
    return &obj->do_Right;
}

const STRPTR DiffObject_OldName(DiffObject *obj)
{
    if (!obj || !obj->do_OldFileName)
        return "Original";
    return obj->do_OldFileName;
}

const STRPTR DiffObject_NewName(DiffObject *obj)
{
    if (!obj || !obj->do_NewFileName)
        return "Modified";
    return obj->do_NewFileName;
}

ULONG DiffObject_MaxLineLen(DiffObject *obj)
{
    if (!obj)
        return 0;
    return obj->do_MaxLineLen;
}
