/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 *
 * diffrender.c - text pane rendering for diffview.gadget.
 *
 * Ported from ADiffView DiffWindowTextArea.cpp and DiffWindowRastports.cpp.
 */

#include "classbase.h"
#include "diffrender.h"
#include "diffline.h"

#ifndef OS30_ONLY
#include <graphics/rpattr.h>
#endif

static void dr_DrawLineText(struct DiffViewClassBase *cb, struct RastPort *rp,
                            DiffPens *pens, DiffLine *line, ULONG lineId,
                            DiffPaneLayout *layout, DiffSelect *sel,
                            DiffFile *file, WORD lineTop)
{
    TextPositionInfo info;
    ULONG resultingCol;
    ULONG maxRemaining;
    ULONG currentCol;
    STRPTR spaces;
    ULONG spacesLen;
    STRPTR text;
    ULONG numPrint;
    ULONG numChars;
    STRPTR pText;
    LONG bpen;
    LONG apen;
    LONG numCharsInBlock;
    UBYTE state;

    if (!cb || !line || !rp || !layout)
        return;

    state = DiffLine_State(line);
    bpen = DiffPens_LineStateBPen(pens, state);
    text = DiffLine_Text(line);

    if (!text || !text[0])
    {
        DiffPens_FillPattern(cb, rp, pens,
                             layout->dpl_TextLeft, lineTop,
                             layout->dpl_TextW, layout->dpl_FontH,
                             pens->dp_Colors.dc_BlankCol,
                             pens->dp_PenBlank, pens->dp_PenBack);
        return;
    }

    resultingCol = layout->dpl_ViewLeft;
    maxRemaining = layout->dpl_MaxChars;
    currentCol = 0;
    spaces = (STRPTR) "                ";
    spacesLen = 16;

    DiffLine_GetTextPositionInfo(line, &info, resultingCol, layout->dpl_TabSize);
    while (maxRemaining > 0 &&
           (info.tpi_RemainingChars > 0 || info.tpi_RemainingSpaces > 0))
    {
        apen = pens->dp_PenText;
        numCharsInBlock = 0;

        if (sel && file)
        {
            numCharsInBlock = DiffSelect_GetNumNormalChars(sel, file, lineId,
                                                         info.tpi_SrcTextColumn);
            if (numCharsInBlock > 0)
            {
                apen = pens->dp_PenText;
            }
            else
            {
                numCharsInBlock = DiffSelect_GetNumMarkedChars(sel, file, lineId,
                                                             info.tpi_SrcTextColumn);
                if (numCharsInBlock > 0)
                    apen = pens->dp_PenHighlight;
                else
                    break;
            }
        }

        numChars = info.tpi_RemainingChars;
        pText = text + info.tpi_SrcTextColumn;
        numPrint = numChars;

        if (info.tpi_RemainingSpaces > 0)
        {
            numPrint = info.tpi_RemainingSpaces;
            if (numPrint > spacesLen)
                numPrint = spacesLen;
            pText = spaces;
            numChars = 1;
        }

        if (sel && file && numCharsInBlock > 0)
        {
            if ((ULONG) numCharsInBlock < numPrint)
                numPrint = (ULONG) numCharsInBlock;
        }

        if (numPrint > maxRemaining)
            numPrint = maxRemaining;

        if (numPrint == 0)
            break;

        DiffPens_SetJam2(cb, rp, apen, bpen);
        Move(rp,
             layout->dpl_TextLeft + layout->dpl_FontW * currentCol,
             lineTop + layout->dpl_FontBase + 1);
        Text(rp, pText, (LONG) numPrint);

        maxRemaining -= numPrint;
        currentCol += numPrint;
        resultingCol += numPrint;

        if (info.tpi_SrcTextColumn + numChars >= DiffLine_TextLen(line))
            break;

        DiffLine_GetTextPositionInfo(line, &info, resultingCol, layout->dpl_TabSize);
    }
}

void DiffRender_ComputePane(DiffPaneLayout *layout, WORD left, WORD top,
                            WORD width, WORD height, struct TextFont *font,
                            ULONG viewTop, ULONG viewLeft, ULONG tabSize)
{
    ULONG lineNumsChars;

    if (!layout)
        return;

    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;

    layout->dpl_Left = left;
    layout->dpl_Top = top;
    layout->dpl_Width = width;
    layout->dpl_Height = height;
    layout->dpl_ViewTop = viewTop;
    layout->dpl_ViewLeft = viewLeft;
    layout->dpl_TabSize = tabSize ? tabSize : 8;

    if (font)
    {
        layout->dpl_FontW = font->tf_XSize;
        layout->dpl_FontH = font->tf_YSize;
        layout->dpl_FontBase = font->tf_Baseline;
    }
    else
    {
        layout->dpl_FontW = 8;
        layout->dpl_FontH = 8;
        layout->dpl_FontBase = 7;
    }

    layout->dpl_MaxLines = (ULONG) (height - 4) / layout->dpl_FontH;
    if (layout->dpl_MaxLines < 1)
        layout->dpl_MaxLines = 1;
    if (layout->dpl_MaxLines > 256)
        layout->dpl_MaxLines = 256;

    lineNumsChars = 5;
    layout->dpl_LineNumsW = (WORD) (lineNumsChars * layout->dpl_FontW);
    layout->dpl_TextLeft = (WORD) (left + layout->dpl_LineNumsW + 2);
    layout->dpl_TextTop = (WORD) (top + 1);
    layout->dpl_TextW = (WORD) (width - 7 - layout->dpl_LineNumsW);
    if (layout->dpl_TextW < layout->dpl_FontW)
        layout->dpl_TextW = layout->dpl_FontW;
    layout->dpl_TextH = (WORD) (layout->dpl_MaxLines * layout->dpl_FontH);
    layout->dpl_MaxChars = layout->dpl_TextW / layout->dpl_FontW;
    if (layout->dpl_MaxChars < 1)
        layout->dpl_MaxChars = 1;
}

void DiffRender_DrawPaneBevel(struct DiffViewClassBase *cb, struct RastPort *rp,
                              DiffPens *pens, DiffPaneLayout *layout)
{
    WORD x;
    WORD y;
    WORD w;
    WORD h;

    if (!cb || !rp || !pens || !layout)
        return;

    x = layout->dpl_Left;
    y = layout->dpl_Top;
    w = layout->dpl_Width;
    h = layout->dpl_Height;

    /* Recessed bevel like ADiffView DrawBevelBox GTBB_Recessed. */
    SetAPen(rp, pens->dp_PenGeneralShadow);
    Move(rp, x, y);
    Draw(rp, x + w - 1, y);
    Move(rp, x, y);
    Draw(rp, x, y + h - 1);

    SetAPen(rp, pens->dp_PenGeneralShine);
    Move(rp, x + 1, y + h - 1);
    Draw(rp, x + w - 1, y + h - 1);
    Move(rp, x + w - 1, y + 1);
    Draw(rp, x + w - 1, y + h - 1);
}

void DiffRender_DrawPane(struct DiffViewClassBase *cb, struct RastPort *rp,
                         DiffPens *pens, DiffFile *file,
                         DiffPaneLayout *layout, DiffSelect *sel,
                         ULONG currentLine)
{
    ULONG lineId;
    ULONG i;
    WORD lineTop;
    DiffLine *line;
    STRPTR lineNum;
    ULONG lineNumLen;
    WORD bx0;
    WORD by0;
    WORD bx1;
    WORD by1;

    if (!cb || !rp || !pens || !file || !layout)
        return;

    DiffRender_DrawPaneBevel(cb, rp, pens, layout);

    DiffPens_FillRect(cb, rp, pens,
                      layout->dpl_Left + 2, layout->dpl_Top + 1,
                      (LONG) (layout->dpl_Width - 4),
                      (LONG) (layout->dpl_Height - 2),
                      pens->dp_Colors.dc_BackCol, pens->dp_PenBack);

    for (i = 0; i < layout->dpl_MaxLines; i++)
    {
        lineId = layout->dpl_ViewTop + i;
        line = DiffFile_Line(file, lineId);
        lineTop = (WORD) (layout->dpl_TextTop + i * layout->dpl_FontH);

        if (!line)
            continue;

        DiffPens_SetJam2(cb, rp, pens->dp_PenLineNoText, pens->dp_PenLineNoBack);
        lineNum = DiffLine_LineNumText(line);
        lineNumLen = (ULONG) strlen(lineNum);
        Move(rp, layout->dpl_Left + 2, lineTop + layout->dpl_FontBase + 1);
        Text(rp, lineNum, (LONG) lineNumLen);

        dr_DrawLineText(cb, rp, pens, line, lineId, layout, sel, file, lineTop);

        if (currentLine > 0 && lineId + 1 == currentLine)
        {
            bx0 = (WORD) (layout->dpl_Left + 1);
            by0 = (WORD) (lineTop - 1);
            bx1 = (WORD) (layout->dpl_Left + layout->dpl_Width - 2);
            by1 = (WORD) (lineTop + layout->dpl_FontH);
            SetAPen(rp, pens->dp_PenCurrent);
            SetDrMd(rp, JAM1);
            Move(rp, bx0, by0);
            Draw(rp, bx1, by0);
            Draw(rp, bx1, by1);
            Draw(rp, bx0, by1);
            Draw(rp, bx0, by0);
        }
    }
}

void DiffRender_DrawOverview(struct DiffViewClassBase *cb, struct RastPort *rp,
                             DiffPens *pens, DiffFile *left, DiffFile *right,
                             WORD leftX, WORD top, WORD width, WORD height,
                             ULONG viewTop, ULONG viewLines)
{
    ULONG numLines;
    ULONG i;
    WORD x0;
    WORD x1;
    WORD y;
    WORD barH;
    LONG pen;
    DiffLine *ll;
    DiffLine *rl;
    UBYTE st;

    if (!cb || !rp || !pens || !left)
        return;

    numLines = DiffFile_NumLines(left);
    if (right && DiffFile_NumLines(right) > numLines)
        numLines = DiffFile_NumLines(right);
    if (numLines < 1)
        numLines = 1;

    DiffPens_FillRect(cb, rp, pens, leftX, top, width, height,
                      0, pens->dp_PenBack);

    x0 = (WORD) (leftX + 2);
    x1 = (WORD) (leftX + width / 2 - 2);
    barH = (WORD) (height - 4);
    if (barH < 1)
        barH = 1;

    for (i = 0; i < numLines; i++)
    {
        ll = DiffFile_Line(left, i);
        rl = right ? DiffFile_Line(right, i) : NULL;
        st = DLS_NORMAL;
        if (ll)
            st = DiffLine_State(ll);
        else if (rl)
            st = DiffLine_State(rl);

        if (st == DLS_NORMAL)
            continue;

        if (st == DLS_DELETED)
            pen = pens->dp_PenOverviewOld;
        else if (st == DLS_ADDED)
            pen = pens->dp_PenOverviewNew;
        else if (st == DLS_WS_ONLY)
            pen = pens->dp_PenOverviewWs;
        else
            pen = pens->dp_PenWs;

        y = (WORD) (top + 2 + (i * barH) / numLines);
        DiffPens_FillRect(cb, rp, pens, x0, y, x1 - x0, 1, 0, pen);
        DiffPens_FillRect(cb, rp, pens, (WORD) (x1 + 4), y,
                          (WORD) (leftX + width - x1 - 6), 1, 0, pen);
    }

    if (viewLines > 0 && numLines > viewLines)
    {
        WORD vy;
        WORD vh;

        vy = (WORD) (top + 2 + (viewTop * barH) / numLines);
        vh = (WORD) ((viewLines * barH) / numLines);
        if (vh < 2)
            vh = 2;
        SetAPen(rp, pens->dp_PenGeneralShadow);
        RectFill(rp, leftX + 1, vy, leftX + width - 2, vy + vh - 1);
    }
}

void DiffRender_DrawHeader(struct DiffViewClassBase *cb, struct RastPort *rp,
                           DiffPens *pens, const STRPTR oldName,
                           const STRPTR newName, WORD left, WORD top,
                           WORD width, WORD height, struct TextFont *font)
{
    WORD half;

    if (!cb || !rp || !pens)
        return;

    DiffPens_FillRect(cb, rp, pens, left, top, width, height,
                      0, pens->dp_PenBack);

    half = (WORD) (width / 2);
    DiffPens_SetJam2(cb, rp, pens->dp_PenText, pens->dp_PenBack);
    if (font)
        SetFont(rp, font);
    Move(rp, left + 4, top + height - 4);
    Text(rp, oldName, (LONG) strlen(oldName));
    Move(rp, left + half + 4, top + height - 4);
    Text(rp, newName, (LONG) strlen(newName));
}

static void dr_AppendULong(STRPTR buf, ULONG *pos, ULONG value)
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
        buf[p++] = tmp[i - 1];
    *pos = p;
}

static void dr_AppendStr(STRPTR buf, ULONG *pos, CONST_STRPTR text)
{
    ULONG p;
    ULONG i;

    if (!text)
        return;

    p = *pos;
    for (i = 0; text[i] != '\0'; i++)
        buf[p++] = text[i];
    *pos = p;
}

void DiffRender_CountStats(DiffFile *left, DiffFile *right, DiffViewStats *stats)
{
    ULONG i;
    ULONG n;
    DiffLine *line;
    UBYTE st;

    if (!stats)
        return;

    stats->dvs_NumAdded = 0;
    stats->dvs_NumChanged = 0;
    stats->dvs_NumDeleted = 0;

    if (left)
    {
        n = DiffFile_NumLines(left);
        for (i = 0; i < n; i++)
        {
            line = DiffFile_Line(left, i);
            if (!line)
                continue;
            st = DiffLine_State(line);
            if (st == DLS_DELETED)
                stats->dvs_NumDeleted++;
            else if (st == DLS_CHANGED || st == DLS_WS_ONLY)
                stats->dvs_NumChanged++;
        }
    }

    if (right)
    {
        n = DiffFile_NumLines(right);
        for (i = 0; i < n; i++)
        {
            line = DiffFile_Line(right, i);
            if (!line)
                continue;
            st = DiffLine_State(line);
            if (st == DLS_ADDED)
                stats->dvs_NumAdded++;
        }
    }
}

void DiffRender_DrawStatusBar(struct DiffViewClassBase *cb, struct RastPort *rp,
                              DiffPens *pens, WORD left, WORD top, WORD width,
                              WORD height, struct TextFont *font,
                              const DiffViewStats *stats)
{
    STRPTR buf;
    ULONG pos;
    struct IntuiText it;

    if (!cb || !rp || !pens || !stats)
        return;

    DiffPens_FillRect(cb, rp, pens, left, top, width, height,
                      0, pens->dp_PenBack);

    buf = (STRPTR) AllocMem(96, MEMF_CLEAR);
    if (!buf)
        return;

    pos = 0;
    dr_AppendStr(buf, &pos, (STRPTR) "Added: ");
    dr_AppendULong(buf, &pos, stats->dvs_NumAdded);
    dr_AppendStr(buf, &pos, (STRPTR) "  Changed: ");
    dr_AppendULong(buf, &pos, stats->dvs_NumChanged);
    dr_AppendStr(buf, &pos, (STRPTR) "  Deleted: ");
    dr_AppendULong(buf, &pos, stats->dvs_NumDeleted);
    buf[pos] = '\0';

    it.FrontPen = pens->dp_PenText;
    it.BackPen = pens->dp_PenBack;
    it.DrawMode = JAM2;
    it.ITextFont = font;
    if (!it.ITextFont)
        it.ITextFont = (struct TextFont *) rp->Font;
    it.IText = buf;
    it.NextText = NULL;
    it.LeftEdge = left + 4;
    it.TopEdge = (WORD) (top + height - (font ? font->tf_Baseline : 7) - 1);

    PrintIText(rp, &it, 0, 0);
    FreeMem(buf, 96);
}

BOOL DiffRender_PaneHit(const DiffPaneLayout *layout, WORD x, WORD y)
{
    if (!layout)
        return FALSE;

    if (x < layout->dpl_Left + 2 || x >= layout->dpl_Left + layout->dpl_Width - 2)
        return FALSE;
    if (y < layout->dpl_Top + 1 || y >= layout->dpl_Top + layout->dpl_Height - 1)
        return FALSE;

    return TRUE;
}

BOOL DiffRender_PaneMousePos(const DiffPaneLayout *layout, WORD x, WORD y,
                             ULONG *lineId, ULONG *docColumn, ULONG tabSize,
                             DiffFile *file)
{
    LONG relX;
    LONG relY;
    ULONG renderCol;
    DiffLine *line;

    if (!layout || !lineId || !docColumn || !file)
        return FALSE;

    if (!DiffRender_PaneHit(layout, x, y))
        return FALSE;

    relX = (LONG) x - (LONG) layout->dpl_Left - 2;
    relY = (LONG) y - (LONG) layout->dpl_TextTop;
    if (relY < 0)
        relY = 0;

    *lineId = layout->dpl_ViewTop + (ULONG) (relY / layout->dpl_FontH);
    if (*lineId >= DiffFile_NumLines(file))
        *lineId = DiffFile_NumLines(file) ? DiffFile_NumLines(file) - 1 : 0;

    renderCol = layout->dpl_ViewLeft +
                (ULONG) (relX / layout->dpl_FontW);
    line = DiffFile_Line(file, *lineId);
    if (!line)
    {
        *docColumn = 0;
        return TRUE;
    }

    *docColumn = DiffLine_DocumentColumn(line, renderCol, tabSize);
    return TRUE;
}

ULONG DiffRender_OverviewLineAt(WORD mouseY, WORD top, WORD height,
                                ULONG numLines)
{
    LONG yRel;
    WORD barH;
    ULONG lineId;

    if (numLines < 1)
        numLines = 1;

    barH = (WORD) (height - 4);
    if (barH < 1)
        barH = 1;

    yRel = (LONG) mouseY - (LONG) top - 2;
    if (yRel < 0)
        yRel = 0;

    lineId = (ULONG) ((ULONG) yRel * numLines / (ULONG) barH);
    if (lineId >= numLines)
        lineId = numLines - 1;

    return lineId;
}
