/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 *
 * diffselect.c - mouse text selection for diffview.gadget.
 *
 * Ported from ADiffView DynamicSelection and SelectableDiffFile.
 */

#include "diffselect.h"
#include "diffline.h"

#define DS_MIN2(a, b) ((LONG) ((a) < (b) ? (a) : (b)))
#define DS_MAX2(a, b) ((LONG) ((a) > (b) ? (a) : (b)))

static LONG ds_LineChars(DiffFile *file, ULONG lineId)
{
    DiffLine *line;

    if (!file)
        return 0;

    line = DiffFile_Line(file, lineId);
    if (!line)
        return 0;

    return (LONG) DiffLine_TextLen(line);
}

void DiffSelect_Init(DiffSelect *sel)
{
    if (!sel)
        return;

    sel->ds_StartLineId = -1;
    sel->ds_StartColumnId = -1;
    sel->ds_MinLineId = -1;
    sel->ds_MinLineColumnId = -1;
    sel->ds_MaxLineId = -1;
    sel->ds_MaxLineColumnId = -1;
}

void DiffSelect_Clear(DiffSelect *sel)
{
    if (!sel)
        return;

    DiffSelect_Init(sel);
}

void DiffSelect_Start(DiffSelect *sel, DiffFile *file,
                      ULONG lineId, ULONG columnId)
{
    LONG lastColumn;

    if (!sel || !file)
        return;

    if (lineId >= DiffFile_NumLines(file))
        return;

    DiffSelect_Clear(sel);

    lastColumn = ds_LineChars(file, lineId);
    if ((LONG) columnId > lastColumn)
        columnId = (ULONG) lastColumn;

    sel->ds_StartLineId = (LONG) lineId;
    sel->ds_MinLineId = (LONG) lineId;
    sel->ds_MaxLineId = (LONG) lineId;
    sel->ds_StartColumnId = (LONG) columnId;
    sel->ds_MinLineColumnId = (LONG) columnId;
    sel->ds_MaxLineColumnId = (LONG) columnId;
}

void DiffSelect_Update(DiffSelect *sel, DiffFile *file,
                       ULONG lineId, ULONG columnId)
{
    LONG maxAllowed;

    if (!sel || !file || sel->ds_StartLineId < 0)
        return;

    if (lineId >= DiffFile_NumLines(file))
        return;

    maxAllowed = ds_LineChars(file, lineId);
    if ((LONG) columnId > maxAllowed)
        columnId = (ULONG) maxAllowed;

    if (((LONG) lineId < sel->ds_MinLineId) ||
        (sel->ds_MinLineId < sel->ds_StartLineId &&
         (LONG) lineId <= sel->ds_StartLineId &&
         sel->ds_MinLineId != (LONG) lineId))
    {
        sel->ds_MinLineId = (LONG) lineId;
        sel->ds_MinLineColumnId = (LONG) columnId;
        sel->ds_MaxLineId = sel->ds_StartLineId;
        sel->ds_MaxLineColumnId = sel->ds_StartColumnId;
        return;
    }

    if (((LONG) lineId > sel->ds_MaxLineId) ||
        (sel->ds_MaxLineId > sel->ds_StartLineId &&
         (LONG) lineId >= sel->ds_StartLineId &&
         sel->ds_MaxLineId != (LONG) lineId))
    {
        sel->ds_MaxLineId = (LONG) lineId;
        sel->ds_MaxLineColumnId = (LONG) columnId;
        sel->ds_MinLineId = sel->ds_StartLineId;
        sel->ds_MinLineColumnId = sel->ds_StartColumnId;
        return;
    }

    if (sel->ds_MinLineId == sel->ds_MaxLineId)
    {
        if ((LONG) columnId != sel->ds_MinLineColumnId)
        {
            sel->ds_MinLineColumnId = (LONG) columnId;
            sel->ds_MaxLineColumnId = (LONG) columnId;
        }
        return;
    }

    if ((LONG) lineId == sel->ds_MinLineId)
    {
        if ((LONG) columnId != sel->ds_MinLineColumnId)
            sel->ds_MinLineColumnId = (LONG) columnId;
        return;
    }

    if ((LONG) lineId == sel->ds_MaxLineId)
    {
        if ((LONG) columnId != sel->ds_MaxLineColumnId)
            sel->ds_MaxLineColumnId = (LONG) columnId;
    }
}

BOOL DiffSelect_IsSelected(const DiffSelect *sel, DiffFile *file,
                           ULONG lineId, ULONG columnId)
{
    LONG from;
    LONG to;

    if (!sel || !file || sel->ds_MinLineId < 0)
        return FALSE;

    if ((LONG) lineId < sel->ds_MinLineId || (LONG) lineId > sel->ds_MaxLineId)
        return FALSE;

    if ((LONG) lineId == sel->ds_MinLineId)
    {
        if (sel->ds_MinLineId == sel->ds_MaxLineId)
        {
            from = DS_MIN2(sel->ds_MinLineColumnId, sel->ds_StartColumnId);
            to = DS_MAX2(sel->ds_MinLineColumnId, sel->ds_StartColumnId);
            if ((LONG) columnId >= from && (LONG) columnId <= to)
                return TRUE;
            return FALSE;
        }

        if ((LONG) columnId > sel->ds_MaxLineColumnId &&
            (LONG) columnId <= ds_LineChars(file, lineId))
            return TRUE;
        return FALSE;
    }

    if ((LONG) lineId == sel->ds_MaxLineId)
    {
        if ((LONG) columnId <= sel->ds_MaxLineColumnId)
            return TRUE;
        return FALSE;
    }

    if ((LONG) columnId <= ds_LineChars(file, lineId))
        return TRUE;

    return FALSE;
}

LONG DiffSelect_GetNumMarkedChars(const DiffSelect *sel, DiffFile *file,
                                  ULONG lineId, ULONG columnId)
{
    LONG fromColumn;
    LONG toColumn;
    LONG lineTextLength;

    if (!sel || !file || sel->ds_MinLineId < 0)
        return 0;

    if ((LONG) lineId < sel->ds_MinLineId || (LONG) lineId > sel->ds_MaxLineId)
        return 0;

    if (sel->ds_MinLineId == sel->ds_MaxLineId)
    {
        fromColumn = DS_MIN2(DS_MIN2(sel->ds_MinLineColumnId, sel->ds_MaxLineColumnId),
                             sel->ds_StartColumnId);
        toColumn = DS_MAX2(DS_MAX2(sel->ds_MinLineColumnId, sel->ds_MaxLineColumnId),
                           sel->ds_StartColumnId);
        if ((LONG) columnId < fromColumn || (LONG) columnId > toColumn)
            return 0;
        return toColumn - (LONG) columnId;
    }

    if ((LONG) lineId == sel->ds_MinLineId)
    {
        if ((LONG) columnId < sel->ds_MinLineColumnId)
            return 0;
        return ds_LineChars(file, lineId) - (LONG) columnId;
    }

    if ((LONG) lineId == sel->ds_MaxLineId)
    {
        if ((LONG) columnId > sel->ds_MaxLineColumnId)
            return 0;
        return sel->ds_MaxLineColumnId - (LONG) columnId;
    }

    lineTextLength = ds_LineChars(file, lineId);
    if ((LONG) columnId > lineTextLength)
        return -1;

    return lineTextLength - (LONG) columnId;
}

LONG DiffSelect_GetNextSelectionStart(const DiffSelect *sel, DiffFile *file,
                                      ULONG lineId, ULONG columnId)
{
    if (!sel || !file || sel->ds_MinLineId < 0)
        return -1;

    if ((LONG) lineId < sel->ds_MinLineId || (LONG) lineId > sel->ds_MaxLineId)
        return -1;

    if (sel->ds_MinLineId == sel->ds_MaxLineId)
    {
        if ((LONG) columnId > sel->ds_MinLineColumnId)
            return -1;
        return DS_MIN2(sel->ds_MinLineColumnId, sel->ds_StartColumnId) -
               (LONG) columnId;
    }

    if ((LONG) lineId == sel->ds_MinLineId)
    {
        if ((LONG) columnId > sel->ds_MinLineColumnId)
            return -1;
        return sel->ds_MinLineColumnId - (LONG) columnId;
    }

    if ((LONG) lineId == sel->ds_MaxLineId)
    {
        if ((LONG) columnId > sel->ds_MaxLineColumnId)
            return -1;
        return 0;
    }

    return 0;
}

LONG DiffSelect_GetNumNormalChars(DiffSelect *sel, DiffFile *file,
                                  ULONG lineId, ULONG columnId)
{
    LONG numMarked;
    LONG nextSelStart;
    LONG lineChars;

    if (!sel || !file)
        return 0;

    if (lineId >= DiffFile_NumLines(file))
        return 0;

    lineChars = ds_LineChars(file, lineId);
    if ((LONG) columnId > lineChars)
        return 0;

    numMarked = DiffSelect_GetNumMarkedChars(sel, file, lineId, columnId);
    if (numMarked > 0)
        return 0;

    nextSelStart = DiffSelect_GetNextSelectionStart(sel, file, lineId, columnId);
    if (nextSelStart > 0)
        return nextSelStart;

    return lineChars - (LONG) columnId;
}

ULONG DiffSelect_GetTotalChars(const DiffSelect *sel, DiffFile *file)
{
    ULONG total;
    LONG i;
    LONG startCol;

    if (!sel || !file || sel->ds_MinLineId < 0)
        return 0;

    total = 0;
    for (i = sel->ds_MinLineId; i <= sel->ds_MaxLineId; i++)
    {
        if (i == sel->ds_MinLineId || i == sel->ds_MaxLineId)
        {
            startCol = DiffSelect_GetNextSelectionStart(sel, file,
                                                      (ULONG) i, 0);
            if (startCol < 0)
                startCol = 0;
            total += (ULONG) DiffSelect_GetNumMarkedChars(sel, file,
                                                        (ULONG) i,
                                                        (ULONG) startCol);
        }
        else
        {
            total += (ULONG) ds_LineChars(file, (ULONG) i);
        }
    }

    return total;
}
