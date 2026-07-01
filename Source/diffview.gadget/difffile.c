/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 *
 * difffile.c - growable array of DiffLine pointers.
 */

#include <string.h>

#include "classbase.h"
#include "difffile.h"

#define DF_GROW 64

static void df_FormatLineNum(STRPTR buf, ULONG value, ULONG width)
{
    ULONG v;
    ULONG numDigits;
    ULONG pos;
    ULONG i;

    numDigits = DiffFile_NumDigits(value);
    pos = 0;
    for (i = numDigits; i < width; i++)
        buf[pos++] = ' ';

    v = value;
    i = numDigits;
    while (i > 0)
    {
        i--;
        buf[pos + i] = (char) ('0' + (v % 10));
        v /= 10;
    }
    pos += numDigits;
    buf[pos++] = ' ';
    buf[pos] = '\0';
}

void DiffFile_Init(DiffFile *file)
{
    if (!file)
        return;
    file->df_Lines = NULL;
    file->df_NumLines = 0;
    file->df_Capacity = 0;
}

void DiffFile_Free(struct DiffViewClassBase *cb, DiffFile *file)
{
    ULONG i;

    if (!file)
        return;

    for (i = 0; i < file->df_NumLines; i++)
    {
        if (file->df_Lines[i])
        {
            DiffLine_Free(cb, file->df_Lines[i]);
            FreeMem(file->df_Lines[i], sizeof(DiffLine));
            file->df_Lines[i] = NULL;
        }
    }
    if (file->df_Lines)
    {
        FreeMem(file->df_Lines, file->df_Capacity * sizeof(DiffLine *));
        file->df_Lines = NULL;
    }
    file->df_NumLines = 0;
    file->df_Capacity = 0;
}

BOOL DiffFile_AddLine(struct DiffViewClassBase *cb, DiffFile *file,
                      DiffLine *line)
{
    DiffLine **newLines;
    ULONG newCap;

    if (!file || !line)
        return FALSE;

    if (file->df_NumLines >= file->df_Capacity)
    {
        newCap = file->df_Capacity + DF_GROW;
        newLines = (DiffLine **) AllocMem(newCap * sizeof(DiffLine *),
                                          MEMF_CLEAR);
        if (!newLines)
            return FALSE;
        if (file->df_Lines)
        {
            memcpy(newLines, file->df_Lines,
                   file->df_NumLines * sizeof(DiffLine *));
            FreeMem(file->df_Lines, file->df_Capacity * sizeof(DiffLine *));
        }
        file->df_Lines = newLines;
        file->df_Capacity = newCap;
    }

    file->df_Lines[file->df_NumLines++] = line;
    return TRUE;
}

BOOL DiffFile_AddEmptyLine(struct DiffViewClassBase *cb, DiffFile *file)
{
    DiffLine *line;

    line = (DiffLine *) AllocMem(sizeof(DiffLine), MEMF_CLEAR);
    if (!line)
        return FALSE;

    DiffLine_InitLinked(line, "", DLS_NORMAL, NULL);
    return DiffFile_AddLine(cb, file, line);
}

DiffLine *DiffFile_Line(DiffFile *file, ULONG index)
{
    if (!file || index >= file->df_NumLines)
        return NULL;
    return file->df_Lines[index];
}

ULONG DiffFile_NumLines(const DiffFile *file)
{
    if (!file)
        return 0;
    return file->df_NumLines;
}

ULONG DiffFile_NumDigits(ULONG number)
{
    ULONG digits;

    digits = 1;
    if (number >= 100000000UL) { digits += 8; number /= 100000000UL; }
    if (number >= 10000UL)     { digits += 4; number /= 10000UL; }
    if (number >= 100UL)       { digits += 2; number /= 100UL; }
    if (number >= 10UL)        { digits += 1; }
    return digits;
}

BOOL DiffFile_CollectLineNumbers(struct DiffViewClassBase *cb, DiffFile *file,
                                 ULONG maxLines)
{
    ULONG digits;
    ULONG i;
    STRPTR numBuf;
    DiffLine *line;

    if (!file)
        return FALSE;

    digits = DiffFile_NumDigits(maxLines ? maxLines : file->df_NumLines);
    for (i = 0; i < file->df_NumLines; i++)
    {
        line = file->df_Lines[i];
        if (!line)
            continue;

        numBuf = (STRPTR) AllocMem(digits + 3, MEMF_CLEAR);
        if (!numBuf)
            return FALSE;
        df_FormatLineNum(numBuf, i + 1, digits);
        DiffLine_SetLineNumText(line, numBuf);
    }
    return TRUE;
}

ULONG DiffFile_MaxRenderWidth(const DiffFile *file, ULONG tabSize)
{
    ULONG i;
    ULONG maxWidth;
    ULONG width;
    DiffLine *line;

    maxWidth = 0;
    if (!file)
        return 0;

    for (i = 0; i < file->df_NumLines; i++)
    {
        line = file->df_Lines[i];
        if (!line)
            continue;
        width = DiffLine_RenderColumn(line, DiffLine_TextLen(line), tabSize);
        if (width > maxWidth)
            maxWidth = width;
    }
    return maxWidth;
}
