/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 *
 * diffline.c - single diff line helpers.
 */

#include <string.h>

#include "classbase.h"
#include "diffline.h"

static const STRPTR dl_Empty = (STRPTR) "";

void DiffLine_Init(DiffLine *line, STRPTR text, UBYTE skipLeading)
{
    STRPTR pBuf;
    ULONG i;
    ULONG token;

    if (!line)
        return;

    line->dl_Text = text;
    line->dl_TextLen = text ? (ULONG) strlen(text) : 0;
    line->dl_State = DLS_NORMAL;
    line->dl_LineNumText = NULL;
    line->dl_Linked = 0;
    token = 0;
    pBuf = text;

    if (skipLeading && text)
    {
        while (*pBuf == ' ' || *pBuf == '\t')
            pBuf++;
    }

    for (i = (ULONG) (pBuf - text); i < line->dl_TextLen; i++)
    {
        token = 2 * token + (ULONG) (UBYTE) pBuf[0];
        pBuf++;
    }

    line->dl_Token = token;
}

void DiffLine_InitLinked(DiffLine *line, STRPTR text, UBYTE state,
                         STRPTR lineNumText)
{
    if (!line)
        return;

    line->dl_Text = text;
    line->dl_TextLen = text ? (ULONG) strlen(text) : 0;
    line->dl_State = state;
    line->dl_LineNumText = lineNumText;
    line->dl_Token = 0;
    line->dl_Linked = 1;
}

void DiffLine_Free(struct DiffViewClassBase *cb, DiffLine *line)
{
    if (!line)
        return;

    if (!line->dl_Linked && line->dl_Text)
    {
        FreeMem(line->dl_Text, line->dl_TextLen + 1);
        line->dl_Text = NULL;
    }
    if (line->dl_LineNumText)
    {
        FreeMem(line->dl_LineNumText, strlen(line->dl_LineNumText) + 1);
        line->dl_LineNumText = NULL;
    }
}

const STRPTR DiffLine_Text(const DiffLine *line)
{
    if (!line)
        return dl_Empty;
    return line->dl_Text ? line->dl_Text : dl_Empty;
}

ULONG DiffLine_TextLen(const DiffLine *line)
{
    if (!line)
        return 0;
    return line->dl_TextLen;
}

UBYTE DiffLine_State(const DiffLine *line)
{
    if (!line)
        return DLS_NORMAL;
    return line->dl_State;
}

void DiffLine_SetState(DiffLine *line, UBYTE state)
{
    if (line)
        line->dl_State = state;
}

const STRPTR DiffLine_LineNumText(const DiffLine *line)
{
    if (!line || !line->dl_LineNumText)
        return "   ";
    return line->dl_LineNumText;
}

void DiffLine_SetLineNumText(DiffLine *line, STRPTR text)
{
    if (line)
        line->dl_LineNumText = text;
}

ULONG DiffLine_Token(const DiffLine *line)
{
    if (!line)
        return 0;
    return line->dl_Token;
}

ULONG DiffLine_RenderColumn(const DiffLine *line, ULONG docCol, ULONG tabSize)
{
    ULONG renderColumn;
    ULONG i;

    if (!line || docCol > line->dl_TextLen)
        return 0;

    renderColumn = 0;
    for (i = 0; i < docCol && i < line->dl_TextLen; i++)
    {
        if (line->dl_Text[i] == '\t')
            renderColumn += tabSize - (renderColumn % tabSize);
        else
            renderColumn++;
    }
    return renderColumn;
}

ULONG DiffLine_DocumentColumn(const DiffLine *line, ULONG requestedRenderCol,
                              ULONG tabSize)
{
    ULONG actualRenderColumn;
    ULONG documentColumn;
    UBYTE isTabBorder;
    ULONG tabRemaining;

    if (!line)
        return 0;

    actualRenderColumn = 0;
    isTabBorder = 0;
    for (documentColumn = 0; documentColumn < line->dl_TextLen; documentColumn++)
    {
        if (line->dl_Text[documentColumn] == '\t')
        {
            tabRemaining = tabSize - (documentColumn % tabSize);
            while (tabRemaining-- > 0)
            {
                if (actualRenderColumn == requestedRenderCol)
                    return documentColumn;
                actualRenderColumn++;
            }
            isTabBorder = 1;
        }

        if (actualRenderColumn == requestedRenderCol)
        {
            if (isTabBorder)
                return documentColumn + 1;
            return documentColumn;
        }

        actualRenderColumn++;
        isTabBorder = 0;
    }

    return line->dl_TextLen;
}

void DiffLine_GetTextPositionInfo(const DiffLine *line, TextPositionInfo *info,
                                  ULONG resultingCol, ULONG tabSize)
{
    ULONG accumulatedColumn;
    ULONG nextColumn;
    ULONG i;
    char c;

    if (!line || !info)
        return;

    accumulatedColumn = 0;
    info->tpi_RemainingChars = 0;
    info->tpi_RemainingSpaces = 0;
    info->tpi_SrcTextColumn = 0;

    for (info->tpi_SrcTextColumn = 0;
         info->tpi_SrcTextColumn < line->dl_TextLen;
         info->tpi_SrcTextColumn++)
    {
        c = line->dl_Text[info->tpi_SrcTextColumn];
        if (c == '\t')
            nextColumn = accumulatedColumn + (tabSize - (accumulatedColumn % tabSize));
        else
            nextColumn = accumulatedColumn + 1;

        if (accumulatedColumn > resultingCol)
        {
            info->tpi_SrcTextColumn--;
            info->tpi_RemainingChars = 0;
            info->tpi_RemainingSpaces = tabSize - (resultingCol % tabSize);
            return;
        }
        else if (accumulatedColumn == resultingCol)
        {
            if (c == '\t')
            {
                info->tpi_RemainingChars = 0;
                info->tpi_RemainingSpaces = tabSize - (accumulatedColumn % tabSize);
            }
            else
            {
                i = info->tpi_SrcTextColumn;
                while (i < line->dl_TextLen && line->dl_Text[i] != '\t')
                    i++;
                info->tpi_RemainingChars = i - info->tpi_SrcTextColumn;
                info->tpi_RemainingSpaces = 0;
            }
            return;
        }

        accumulatedColumn = nextColumn;
    }

    info->tpi_RemainingChars = 0;
    info->tpi_RemainingSpaces = 0;
}
