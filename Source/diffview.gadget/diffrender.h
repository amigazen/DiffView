#ifndef DIFFRENDER_H
#define DIFFRENDER_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 */


#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include "difffile.h"
#include "diffpens.h"
#include "diffselect.h"

typedef struct DiffPaneLayout
{
    WORD dpl_Left;
    WORD dpl_Top;
    WORD dpl_Width;
    WORD dpl_Height;
    WORD dpl_LineNumsW;
    WORD dpl_TextLeft;
    WORD dpl_TextTop;
    WORD dpl_TextW;
    WORD dpl_TextH;
    WORD dpl_FontW;
    WORD dpl_FontH;
    WORD dpl_FontBase;
    ULONG dpl_ViewTop;
    ULONG dpl_ViewLeft;
    ULONG dpl_TabSize;
    ULONG dpl_MaxLines;
    ULONG dpl_MaxChars;
} DiffPaneLayout;

typedef struct DiffViewStats
{
    ULONG dvs_NumAdded;
    ULONG dvs_NumChanged;
    ULONG dvs_NumDeleted;
} DiffViewStats;

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

void DiffRender_ComputePane(DiffPaneLayout *layout, WORD left, WORD top,
                            WORD width, WORD height, struct TextFont *font,
                            ULONG viewTop, ULONG viewLeft, ULONG tabSize);
void DiffRender_DrawPane(struct DiffViewClassBase *cb, struct RastPort *rp,
                         DiffPens *pens, DiffFile *file, DiffPaneLayout *layout,
                         DiffSelect *sel, ULONG currentLine);
void DiffRender_DrawPaneBevel(struct DiffViewClassBase *cb, struct RastPort *rp,
                              DiffPens *pens, DiffPaneLayout *layout);
void DiffRender_DrawOverview(struct DiffViewClassBase *cb, struct RastPort *rp,
                             DiffPens *pens, DiffFile *left, DiffFile *right,
                             WORD leftX, WORD top, WORD width, WORD height,
                             ULONG viewTop, ULONG viewLines);
void DiffRender_DrawHeader(struct DiffViewClassBase *cb, struct RastPort *rp,
                           DiffPens *pens, const STRPTR oldName,
                           const STRPTR newName, WORD left, WORD top,
                           WORD width, WORD height, struct TextFont *font);
void DiffRender_DrawStatusBar(struct DiffViewClassBase *cb, struct RastPort *rp,
                              DiffPens *pens, WORD left, WORD top, WORD width,
                              WORD height, struct TextFont *font,
                              const DiffViewStats *stats);
void DiffRender_CountStats(DiffFile *left, DiffFile *right, DiffViewStats *stats);

BOOL DiffRender_PaneHit(const DiffPaneLayout *layout, WORD x, WORD y);
BOOL DiffRender_PaneMousePos(const DiffPaneLayout *layout, WORD x, WORD y,
                             ULONG *lineId, ULONG *docColumn, ULONG tabSize,
                             DiffFile *file);
ULONG DiffRender_OverviewLineAt(WORD mouseY, WORD top, WORD height,
                                ULONG numLines);

#endif
