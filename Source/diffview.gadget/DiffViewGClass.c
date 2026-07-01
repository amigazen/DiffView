/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 *
 * DiffViewGClass.c - diffview.gadget BOOPSI class.
 *
 * Side-by-side diff viewer gadget.  Diff data comes from CreateDiffObject();
 * rendering logic is ported from ADiffView's DiffWindowTextArea.
 */

#include "classbase.h"
#include "Debug.h"
#include "diffobject.h"
#include "diffpens.h"
#include "diffrender.h"
#include "diffselect.h"
#include "diffclip.h"

#include <devices/keymap.h>
#include <libraries/keymap.h>

#define DV_HEADER_H     16
#define DV_OVERVIEW_H   8
#define DV_STATUS_H     14
#define DV_BORDER       2

/* Selection drag state (DiffWindow::SelectionMode). */
#define DV_SEL_NONE             0
#define DV_SEL_LEFT_STARTED     1
#define DV_SEL_LEFT_FINISHED    2
#define DV_SEL_RIGHT_STARTED    3
#define DV_SEL_RIGHT_FINISHED   4

/* Auto-scroll while dragging (DiffWindowTextArea::ScrollRequest). */
#define DV_SCROLL_NONE          0
#define DV_SCROLL_UP            1
#define DV_SCROLL_DOWN          2
#define DV_SCROLL_LEFT          3
#define DV_SCROLL_RIGHT         4

struct DiffViewData
{
    DiffObject       *dvd_DiffObj;
    struct Screen    *dvd_Screen;
    struct TextFont  *dvd_Font;
    Object           *dvd_LineCmp;
    Object           *dvd_VertScroller;
    Object           *dvd_HorizScroller;
    ULONG             dvd_CurrentLine;
    ULONG             dvd_ViewOffsetTop;
    ULONG             dvd_ViewOffsetLeft;
    ULONG             dvd_TabSize;
    DiffColors        dvd_Colors;
    DiffPens          dvd_Pens;
    UBYTE             dvd_PensReady;
    UBYTE             dvd_SelMode;
    UBYTE             dvd_Pad[2];
    DiffSelect        dvd_LeftSelect;
    DiffSelect        dvd_RightSelect;
    DiffPaneLayout    dvd_LeftPane;
    DiffPaneLayout    dvd_RightPane;
};

static ULONG ASM DiffViewDispatcher (
    REG (a0) Class *cl,
    REG (a2) Object *o,
    REG (a1) Msg msg);

static ULONG DV_OMNew (Class *cl, struct Gadget *gad, struct opSet *ops);
static void  DV_OMDispose (Class *cl, struct Gadget *gad, Msg msg);
static ULONG DV_OMSet (Class *cl, struct Gadget *gad, struct opSet *ops);
static ULONG DV_OMGet (Class *cl, struct Gadget *gad, struct opGet *opg);
static ULONG DV_GMLayout (Class *cl, struct Gadget *gad, struct gpLayout *gpl);
static ULONG DV_GMRender (Class *cl, struct Gadget *gad, struct gpRender *gpr);
static ULONG DV_GMGoActive (Class *cl, struct Gadget *gad, struct gpInput *gpi);
static ULONG DV_GMHandleInput (Class *cl, struct Gadget *gad, struct gpInput *gpi);
static ULONG DV_GMGoinActive (Class *cl, struct Gadget *gad, struct gpInput *gpi);

static struct DiffViewData *DV_Data (Class *cl, struct Gadget *gad);
static ULONG DV_ApplyTag (struct DiffViewClassBase *cb,
                          struct DiffViewData *dvd, ULONG tag, ULONG data);
static void  DV_EnsurePens (struct DiffViewClassBase *cb,
                            struct DiffViewData *dvd, struct Screen *scr);
static void  DV_ComputeLayout (struct Gadget *gad, struct DiffViewData *dvd);
static void  DV_UpdateLineCmp (struct DiffViewClassBase *cb,
                              struct DiffViewData *dvd);
static void  DV_SyncScrollers (struct DiffViewClassBase *cb,
                               struct DiffViewData *dvd);
static ULONG DV_PullFromScrollers (struct DiffViewClassBase *cb,
                                   struct DiffViewData *dvd);
static void  DV_InvalidatePens (struct DiffViewClassBase *cb,
                                struct DiffViewData *dvd);
static void  DV_SetCurrentLine (struct DiffViewClassBase *cb,
                                struct DiffViewData *dvd, ULONG line);
static void  DV_ClampView (struct DiffViewData *dvd);
static void  DV_Refresh (struct DiffViewClassBase *cb, struct Gadget *gad,
                         struct gpInput *gpi);
static ULONG DV_ScrollBy (struct DiffViewClassBase *cb, struct DiffViewData *dvd,
                          LONG dTop, LONG dLeft);
static ULONG DV_UpdateSelectionDrag (struct DiffViewClassBase *cb,
                                     struct Gadget *gad, struct DiffViewData *dvd,
                                     struct gpInput *gpi, UBYTE isRight);
static void  DV_StartSelection (struct DiffViewData *dvd, UBYTE isRight,
                                ULONG lineId, ULONG docCol);
static void  DV_ClearOtherSelection (struct DiffViewData *dvd, UBYTE isRight);
static ULONG DV_HandleSelectDown (struct DiffViewClassBase *cb, struct Gadget *gad,
                                  struct DiffViewData *dvd, struct gpInput *gpi);
static ULONG DV_HandleSelectUp (struct DiffViewData *dvd);
static ULONG DV_HandleKey (struct DiffViewClassBase *cb, struct Gadget *gad,
                           struct DiffViewData *dvd, struct gpInput *gpi,
                           struct InputEvent *ie);
static void  DV_GotoDiff (struct DiffViewClassBase *cb, struct DiffViewData *dvd,
                          ULONG direction);
static BOOL  DV_CopySelection (struct DiffViewClassBase *cb,
                               struct DiffViewData *dvd);
static ULONG DV_HandleOverviewClick (struct DiffViewClassBase *cb,
                                     struct Gadget *gad,
                                     struct DiffViewData *dvd,
                                     struct gpInput *gpi);

Class *DiffViewCreateClass (struct DiffViewClassBase *cb);
void   DiffViewDeleteClass (struct DiffViewClassBase *cb, Class *cl);

static struct DiffViewData *DV_Data (Class *cl, struct Gadget *gad)
{
    return (struct DiffViewData *) INST_DATA (cl, gad);
}

static ULONG DV_ApplyTag (struct DiffViewClassBase *cb,
                          struct DiffViewData *dvd, ULONG tag, ULONG data)
{
    ULONG redraw;

    redraw = 0;
    if (!dvd)
        return 0;

    switch (tag)
    {
        case DIFFVIEW_DiffObject:
            dvd->dvd_DiffObj = (DiffObject *) data;
            redraw = 1;
            break;
        case DIFFVIEW_Font:
            dvd->dvd_Font = (struct TextFont *) data;
            redraw = 1;
            break;
        case DIFFVIEW_HorizScroller:
            dvd->dvd_HorizScroller = (Object *) data;
            break;
        case DIFFVIEW_LineCmp:
            dvd->dvd_LineCmp = (Object *) data;
            break;
        case DIFFVIEW_Screen:
            if (dvd->dvd_PensReady)
            {
                DiffPens_Free(cb, &dvd->dvd_Pens);
                dvd->dvd_PensReady = 0;
            }
            dvd->dvd_Screen = (struct Screen *) data;
            redraw = 1;
            break;
        case DIFFVIEW_TabSize:
            dvd->dvd_TabSize = data ? data : 8;
            redraw = 1;
            break;
        case DIFFVIEW_VertScroller:
            dvd->dvd_VertScroller = (Object *) data;
            break;
        case DIFFVIEW_CurrentLine:
            DV_SetCurrentLine (cb, dvd, data);
            redraw = 1;
            break;
        case DIFFVIEW_ViewOffsetTop:
            dvd->dvd_ViewOffsetTop = data;
            redraw = 1;
            break;
        case DIFFVIEW_ViewOffsetLeft:
            dvd->dvd_ViewOffsetLeft = data;
            redraw = 1;
            break;
        case DIFFVIEW_BackCol:
            dvd->dvd_Colors.dc_BackCol = data;
            dvd->dvd_Colors.dc_Flags |= DCF_CUSTOM_BACK;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_TextCol:
            dvd->dvd_Colors.dc_TextCol = data;
            dvd->dvd_Colors.dc_Flags |= DCF_CUSTOM_TEXT;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_OldCol:
            dvd->dvd_Colors.dc_OldCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_NewCol:
            dvd->dvd_Colors.dc_NewCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_BlankCol:
            dvd->dvd_Colors.dc_BlankCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_LineNoBackCol:
            dvd->dvd_Colors.dc_LineNoBackCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_LineNoTextCol:
            dvd->dvd_Colors.dc_LineNoTextCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_GeneralBackCol:
            dvd->dvd_Colors.dc_GeneralBackCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_GeneralShineCol:
            dvd->dvd_Colors.dc_GeneralShineCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_GeneralShadowCol:
            dvd->dvd_Colors.dc_GeneralShadowCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_OverviewOldCol:
            dvd->dvd_Colors.dc_OverviewOldCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_OverviewNewCol:
            dvd->dvd_Colors.dc_OverviewNewCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_WsDifferenceCol:
            dvd->dvd_Colors.dc_WsDifferenceCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        case DIFFVIEW_OverviewWsCol:
            dvd->dvd_Colors.dc_OverviewWsCol = data;
            DV_InvalidatePens(cb, dvd);
            redraw = 1;
            break;
        default:
            break;
    }
    return redraw;
}

static void DV_EnsurePens (struct DiffViewClassBase *cb,
                           struct DiffViewData *dvd, struct Screen *scr)
{
    if (!cb || !dvd || dvd->dvd_PensReady || !scr)
        return;

    if (DiffPens_Init(cb, &dvd->dvd_Pens, scr, &dvd->dvd_Colors))
        dvd->dvd_PensReady = 1;
}

static void DV_ComputeLayout (struct Gadget *gad, struct DiffViewData *dvd)
{
    WORD paneTop;
    WORD paneH;
    WORD halfW;

    if (!gad || !dvd)
        return;

    if (gad->Width < 4)
        gad->Width = 4;
    if (gad->Height < (DV_HEADER_H + DV_OVERVIEW_H + DV_STATUS_H + DV_BORDER * 2 + 8))
        gad->Height = (WORD) (DV_HEADER_H + DV_OVERVIEW_H + DV_STATUS_H +
                              DV_BORDER * 2 + 8);

    paneTop = (WORD) (gad->TopEdge + DV_HEADER_H + DV_OVERVIEW_H + DV_BORDER);
    paneH = (WORD) (gad->Height - DV_HEADER_H - DV_OVERVIEW_H -
                    DV_STATUS_H - DV_BORDER * 2);
    if (paneH < 8)
        paneH = 8;
    halfW = (WORD) ((gad->Width - DV_BORDER * 3) / 2);
    if (halfW < 8)
        halfW = 8;

    DiffRender_ComputePane(&dvd->dvd_LeftPane,
                           (WORD) (gad->LeftEdge + DV_BORDER),
                           paneTop, halfW, paneH,
                           dvd->dvd_Font,
                           dvd->dvd_ViewOffsetTop,
                           dvd->dvd_ViewOffsetLeft,
                           dvd->dvd_TabSize);

    DiffRender_ComputePane(&dvd->dvd_RightPane,
                           (WORD) (gad->LeftEdge + DV_BORDER + halfW + DV_BORDER),
                           paneTop, halfW, paneH,
                           dvd->dvd_Font,
                           dvd->dvd_ViewOffsetTop,
                           dvd->dvd_ViewOffsetLeft,
                           dvd->dvd_TabSize);
}

static void DV_UpdateLineCmp (struct DiffViewClassBase *cb,
                              struct DiffViewData *dvd)
{
    DiffFile *left;
    DiffFile *right;
    DiffLine *ll;
    DiffLine *rl;
    ULONG idx;
    STRPTR empty;

    if (!dvd || !dvd->dvd_LineCmp || !dvd->dvd_DiffObj)
        return;

    if (dvd->dvd_CurrentLine < 1)
        return;

    idx = dvd->dvd_CurrentLine - 1;
    left = DiffObject_Left(dvd->dvd_DiffObj);
    right = DiffObject_Right(dvd->dvd_DiffObj);
    ll = DiffFile_Line(left, idx);
    rl = DiffFile_Line(right, idx);
    empty = (STRPTR) "";

    SetGadgetAttrs((struct Gadget *) dvd->dvd_LineCmp, NULL, NULL,
                   LINECMP_OldLine, ll ? (ULONG) DiffLine_Text(ll) : (ULONG) empty,
                   LINECMP_NewLine, rl ? (ULONG) DiffLine_Text(rl) : (ULONG) empty,
                   LINECMP_ViewOffsetLeft, dvd->dvd_ViewOffsetLeft,
                   LINECMP_TabSize, dvd->dvd_TabSize,
                   TAG_END);
}

static void DV_InvalidatePens (struct DiffViewClassBase *cb,
                               struct DiffViewData *dvd)
{
    if (!cb || !dvd || !dvd->dvd_PensReady)
        return;
    DiffPens_Free(cb, &dvd->dvd_Pens);
    dvd->dvd_PensReady = 0;
}

static ULONG DV_PullFromScrollers (struct DiffViewClassBase *cb,
                                   struct DiffViewData *dvd)
{
    ULONG changed;
    ULONG top;

    changed = 0;
    if (!dvd)
        return 0;

    if (dvd->dvd_VertScroller)
    {
        top = 0;
        GetAttr(SCROLLER_Top, (APTR) dvd->dvd_VertScroller, &top);
        if (top != dvd->dvd_ViewOffsetTop)
        {
            dvd->dvd_ViewOffsetTop = top;
            changed = 1;
        }
    }

    if (dvd->dvd_HorizScroller)
    {
        top = 0;
        GetAttr(SCROLLER_Top, (APTR) dvd->dvd_HorizScroller, &top);
        if (top != dvd->dvd_ViewOffsetLeft)
        {
            dvd->dvd_ViewOffsetLeft = top;
            changed = 1;
        }
    }

    if (changed)
    {
        DV_ClampView(dvd);
        DV_UpdateLineCmp(cb, dvd);
    }

    return changed;
}

static void DV_SyncScrollers (struct DiffViewClassBase *cb,
                              struct DiffViewData *dvd)
{
    ULONG total;
    ULONG visible;
    DiffFile *left;

    if (!dvd || !dvd->dvd_DiffObj)
        return;

    left = DiffObject_Left(dvd->dvd_DiffObj);
    total = DiffFile_NumLines(left);
    visible = dvd->dvd_LeftPane.dpl_MaxLines;

    if (dvd->dvd_VertScroller)
    {
        SetGadgetAttrs((struct Gadget *) dvd->dvd_VertScroller, NULL, NULL,
                       SCROLLER_Top, (WORD) dvd->dvd_ViewOffsetTop,
                       SCROLLER_Visible, (WORD) visible,
                       SCROLLER_Total, (WORD) total,
                       TAG_END);
    }

    if (dvd->dvd_HorizScroller)
    {
        ULONG maxCol;

        maxCol = DiffObject_MaxLineLen(dvd->dvd_DiffObj);
        SetGadgetAttrs((struct Gadget *) dvd->dvd_HorizScroller, NULL, NULL,
                       SCROLLER_Top, (WORD) dvd->dvd_ViewOffsetLeft,
                       SCROLLER_Visible, (WORD) dvd->dvd_LeftPane.dpl_MaxChars,
                       SCROLLER_Total, (WORD) maxCol,
                       TAG_END);
    }
}

static void DV_ClampView (struct DiffViewData *dvd)
{
    ULONG total;
    ULONG maxTop;
    ULONG maxLeft;

    if (!dvd)
        return;

    total = 0;
    maxLeft = 0;
    if (dvd->dvd_DiffObj)
    {
        total = DiffFile_NumLines(DiffObject_Left(dvd->dvd_DiffObj));
        maxLeft = DiffObject_MaxLineLen(dvd->dvd_DiffObj);
    }

    if (total < 1)
        total = 1;

    maxTop = 0;
    if (total > dvd->dvd_LeftPane.dpl_MaxLines)
        maxTop = total - dvd->dvd_LeftPane.dpl_MaxLines;

    if (dvd->dvd_ViewOffsetTop > maxTop)
        dvd->dvd_ViewOffsetTop = maxTop;

    if (maxLeft > dvd->dvd_LeftPane.dpl_MaxChars)
        maxLeft -= dvd->dvd_LeftPane.dpl_MaxChars;
    else
        maxLeft = 0;

    if (dvd->dvd_ViewOffsetLeft > maxLeft)
        dvd->dvd_ViewOffsetLeft = maxLeft;
}

static void DV_Refresh (struct DiffViewClassBase *cb, struct Gadget *gad,
                        struct gpInput *gpi)
{
    struct Window *win;

    if (!cb || !gad)
        return;

    win = (gpi && gpi->gpi_GInfo) ? gpi->gpi_GInfo->gi_Window : NULL;
    if (win)
        RefreshGadgets (gad, win, NULL);
}

static ULONG DV_ScrollBy (struct DiffViewClassBase *cb, struct DiffViewData *dvd,
                          LONG dTop, LONG dLeft)
{
    LONG newTop;
    LONG newLeft;

    if (!dvd)
        return 0;

    newTop = (LONG) dvd->dvd_ViewOffsetTop + dTop;
    newLeft = (LONG) dvd->dvd_ViewOffsetLeft + dLeft;
    if (newTop < 0)
        newTop = 0;
    if (newLeft < 0)
        newLeft = 0;

    dvd->dvd_ViewOffsetTop = (ULONG) newTop;
    dvd->dvd_ViewOffsetLeft = (ULONG) newLeft;
    DV_ClampView (dvd);
    DV_SyncScrollers (cb, dvd);
    DV_UpdateLineCmp (cb, dvd);
    return 1;
}

static void DV_SetCurrentLine (struct DiffViewClassBase *cb,
                               struct DiffViewData *dvd, ULONG line)
{
    ULONG max;

    if (!dvd)
        return;

    max = 0;
    if (dvd->dvd_DiffObj)
        max = DiffFile_NumLines(DiffObject_Left(dvd->dvd_DiffObj));

    if (line < 1)
        line = 1;
    if (max > 0 && line > max)
        line = max;

    dvd->dvd_CurrentLine = line;
    DV_UpdateLineCmp(cb, dvd);
}

static void DV_ClearOtherSelection (struct DiffViewData *dvd, UBYTE isRight)
{
    if (!dvd)
        return;

    if (isRight)
        DiffSelect_Clear(&dvd->dvd_LeftSelect);
    else
        DiffSelect_Clear(&dvd->dvd_RightSelect);
}

static void DV_StartSelection (struct DiffViewData *dvd, UBYTE isRight,
                               ULONG lineId, ULONG docCol)
{
    DiffFile *file;
    DiffSelect *sel;

    if (!dvd || !dvd->dvd_DiffObj)
        return;

    file = isRight ? DiffObject_Right(dvd->dvd_DiffObj)
                   : DiffObject_Left(dvd->dvd_DiffObj);
    sel = isRight ? &dvd->dvd_RightSelect : &dvd->dvd_LeftSelect;

    DiffSelect_Start(sel, file, lineId, docCol);
    DV_ClearOtherSelection(dvd, isRight);
}

static ULONG DV_UpdateSelectionDrag (struct DiffViewClassBase *cb,
                                     struct Gadget *gad, struct DiffViewData *dvd,
                                     struct gpInput *gpi, UBYTE isRight)
{
    DiffPaneLayout *layout;
    DiffFile *file;
    DiffSelect *sel;
    ULONG lineId;
    ULONG docCol;
    ULONG scroll;
    LONG bottomLine;
    LONG rightmostCol;
    ULONG renderCol;
    DiffLine *line;

    layout = isRight ? &dvd->dvd_RightPane : &dvd->dvd_LeftPane;
    file = isRight ? DiffObject_Right(dvd->dvd_DiffObj)
                   : DiffObject_Left(dvd->dvd_DiffObj);
    sel = isRight ? &dvd->dvd_RightSelect : &dvd->dvd_LeftSelect;

    scroll = DV_SCROLL_NONE;
    if (!DiffRender_PaneMousePos(layout, gpi->gpi_Mouse.X, gpi->gpi_Mouse.Y,
                                 &lineId, &docCol, dvd->dvd_TabSize, file))
        return DV_SCROLL_NONE;

    bottomLine = (LONG) (dvd->dvd_ViewOffsetTop + layout->dpl_MaxLines - 1);
    rightmostCol = (LONG) (dvd->dvd_ViewOffsetLeft + layout->dpl_MaxChars);

    line = DiffFile_Line(file, lineId);
    if (line)
        renderCol = DiffLine_RenderColumn(line, docCol, dvd->dvd_TabSize);
    else
        renderCol = 0;

    if ((LONG) lineId > bottomLine)
        scroll = DV_SCROLL_UP;
    else if (lineId <= dvd->dvd_ViewOffsetTop && dvd->dvd_ViewOffsetTop > 0)
        scroll = DV_SCROLL_DOWN;
    else if ((LONG) renderCol > rightmostCol)
        scroll = DV_SCROLL_LEFT;
    else if (renderCol < dvd->dvd_ViewOffsetLeft)
        scroll = DV_SCROLL_RIGHT;

    DiffSelect_Update(sel, file, lineId, docCol);
    DV_SetCurrentLine(cb, dvd, lineId + 1);

    if (scroll == DV_SCROLL_UP)
        DV_ScrollBy(cb, dvd, 1, 0);
    else if (scroll == DV_SCROLL_DOWN)
        DV_ScrollBy(cb, dvd, -1, 0);
    else if (scroll == DV_SCROLL_LEFT)
        DV_ScrollBy(cb, dvd, 0, 2);
    else if (scroll == DV_SCROLL_RIGHT)
        DV_ScrollBy(cb, dvd, 0, -2);

    DV_Refresh(cb, gad, gpi);
    return scroll;
}

static ULONG DV_HandleSelectDown (struct DiffViewClassBase *cb, struct Gadget *gad,
                                  struct DiffViewData *dvd, struct gpInput *gpi)
{
    ULONG lineId;
    ULONG docCol;
    UBYTE shiftExtend;
    DiffSelect *sel;
    DiffFile *file;

    if (!dvd->dvd_DiffObj)
        return GMR_REUSE;

    DV_ComputeLayout(gad, dvd);

    if (DV_HandleOverviewClick(cb, gad, dvd, gpi))
        return GMR_REUSE;

    shiftExtend = 0;
    if (gpi->gpi_IEvent &&
        (gpi->gpi_IEvent->ie_Qualifier &
         (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)))
        shiftExtend = 1;

    if (DiffRender_PaneHit(&dvd->dvd_LeftPane, gpi->gpi_Mouse.X, gpi->gpi_Mouse.Y))
    {
        sel = &dvd->dvd_LeftSelect;
        file = DiffObject_Left(dvd->dvd_DiffObj);
        if (DiffRender_PaneMousePos(&dvd->dvd_LeftPane, gpi->gpi_Mouse.X,
                                    gpi->gpi_Mouse.Y, &lineId, &docCol,
                                    dvd->dvd_TabSize, file))
        {
            if (dvd->dvd_SelMode == DV_SEL_LEFT_FINISHED &&
                DiffSelect_IsSelected(sel, file, lineId, docCol))
            {
                DiffSelect_Clear(sel);
                dvd->dvd_SelMode = DV_SEL_NONE;
            }
            else if (dvd->dvd_SelMode == DV_SEL_LEFT_FINISHED && shiftExtend)
            {
                DiffSelect_Update(sel, file, lineId, docCol);
                dvd->dvd_SelMode = DV_SEL_LEFT_STARTED;
            }
            else
            {
                DV_StartSelection(dvd, 0, lineId, docCol);
                dvd->dvd_SelMode = DV_SEL_LEFT_STARTED;
            }
            DV_SetCurrentLine(cb, dvd, lineId + 1);
            DV_Refresh(cb, gad, gpi);
            return GMR_MEACTIVE;
        }
    }
    else if (DiffRender_PaneHit(&dvd->dvd_RightPane, gpi->gpi_Mouse.X,
                                gpi->gpi_Mouse.Y))
    {
        sel = &dvd->dvd_RightSelect;
        file = DiffObject_Right(dvd->dvd_DiffObj);
        if (DiffRender_PaneMousePos(&dvd->dvd_RightPane, gpi->gpi_Mouse.X,
                                    gpi->gpi_Mouse.Y, &lineId, &docCol,
                                    dvd->dvd_TabSize, file))
        {
            if (dvd->dvd_SelMode == DV_SEL_RIGHT_FINISHED &&
                DiffSelect_IsSelected(sel, file, lineId, docCol))
            {
                DiffSelect_Clear(sel);
                dvd->dvd_SelMode = DV_SEL_NONE;
            }
            else if (dvd->dvd_SelMode == DV_SEL_RIGHT_FINISHED && shiftExtend)
            {
                DiffSelect_Update(sel, file, lineId, docCol);
                dvd->dvd_SelMode = DV_SEL_RIGHT_STARTED;
            }
            else
            {
                DV_StartSelection(dvd, 1, lineId, docCol);
                dvd->dvd_SelMode = DV_SEL_RIGHT_STARTED;
            }
            DV_SetCurrentLine(cb, dvd, lineId + 1);
            DV_Refresh(cb, gad, gpi);
            return GMR_MEACTIVE;
        }
    }
    else
    {
        dvd->dvd_SelMode = DV_SEL_NONE;
    }

    return GMR_REUSE;
}

static ULONG DV_HandleSelectUp (struct DiffViewData *dvd)
{
    if (dvd->dvd_SelMode == DV_SEL_LEFT_STARTED)
        dvd->dvd_SelMode = DV_SEL_LEFT_FINISHED;
    else if (dvd->dvd_SelMode == DV_SEL_RIGHT_STARTED)
        dvd->dvd_SelMode = DV_SEL_RIGHT_FINISHED;
    else
        dvd->dvd_SelMode = DV_SEL_NONE;

    return GMR_REUSE;
}

static void DV_GotoDiff (struct DiffViewClassBase *cb, struct DiffViewData *dvd,
                         ULONG direction)
{
    ULONG i;
    ULONG count;
    ULONG current;
    ULONG target;

    if (!dvd || !dvd->dvd_DiffObj || !dvd->dvd_DiffObj->do_DiffPositions)
        return;

    count = dvd->dvd_DiffObj->do_NumDiffPositions;
    if (count < 1)
        return;

    current = dvd->dvd_CurrentLine;
    target = 0;

    if (direction)
    {
        for (i = 0; i < count; i++)
        {
            if (dvd->dvd_DiffObj->do_DiffPositions[i] > current)
            {
                target = dvd->dvd_DiffObj->do_DiffPositions[i];
                break;
            }
        }
        if (target == 0)
            target = dvd->dvd_DiffObj->do_DiffPositions[count - 1];
    }
    else
    {
        for (i = count; i > 0; i--)
        {
            if (dvd->dvd_DiffObj->do_DiffPositions[i - 1] < current)
            {
                target = dvd->dvd_DiffObj->do_DiffPositions[i - 1];
                break;
            }
        }
        if (target == 0)
            target = dvd->dvd_DiffObj->do_DiffPositions[0];
    }

    if (target < 1)
        return;

    DV_SetCurrentLine(cb, dvd, target);
    if (target > 0)
    {
        if (target - 1 < dvd->dvd_ViewOffsetTop)
            dvd->dvd_ViewOffsetTop = target - 1;
        else if (target - 1 >= dvd->dvd_ViewOffsetTop + dvd->dvd_LeftPane.dpl_MaxLines)
            dvd->dvd_ViewOffsetTop = target - dvd->dvd_LeftPane.dpl_MaxLines;

        DV_ClampView(dvd);
        DV_SyncScrollers(cb, dvd);
    }
}

static ULONG DV_HandleKey (struct DiffViewClassBase *cb, struct Gadget *gad,
                           struct DiffViewData *dvd, struct gpInput *gpi,
                           struct InputEvent *ie)
{
    ULONG page;
    ULONG redraw;
    UWORD code;

    redraw = 0;
    page = dvd->dvd_LeftPane.dpl_MaxLines;

    if (ie->ie_Class != IECLASS_RAWKEY)
        return GMR_REUSE;

    code = (UWORD) (ie->ie_Code & (~IECODE_UP_PREFIX));

    if ((ie->ie_Qualifier & IEQUALIFIER_CONTROL) && code == 0x22)
    {
        DV_CopySelection(cb, dvd);
        return GMR_REUSE;
    }

    if (code == RAWKEY_CRSRUP)
        redraw = DV_ScrollBy(cb, dvd, -2, 0);
    else if (code == RAWKEY_CRSRDOWN)
        redraw = DV_ScrollBy(cb, dvd, 2, 0);
    else if (code == RAWKEY_CRSRLEFT)
        redraw = DV_ScrollBy(cb, dvd, 0, -2);
    else if (code == RAWKEY_CRSRRIGHT)
        redraw = DV_ScrollBy(cb, dvd, 0, 2);
    else if (code == RAWKEY_SPACE)
        redraw = DV_ScrollBy(cb, dvd, (LONG) page, 0);
    else if (code == RAWKEY_BACKSPACE)
        redraw = DV_ScrollBy(cb, dvd, -(LONG) page, 0);
    else if (code == 0x2D)
    {
        DV_GotoDiff(cb, dvd, 1);
        redraw = 1;
    }
    else if (code == 0x30)
    {
        DV_GotoDiff(cb, dvd, 0);
        redraw = 1;
    }
    else if (code == RAWKEY_HELP)
    {
        dvd->dvd_ViewOffsetTop = 0;
        DV_ClampView(dvd);
        DV_SyncScrollers(cb, dvd);
        redraw = 1;
    }
    else if (code == RAWKEY_HOME)
    {
        dvd->dvd_ViewOffsetTop = 0;
        DV_ClampView(dvd);
        DV_SyncScrollers(cb, dvd);
        redraw = 1;
    }
    else if (code == RAWKEY_END)
    {
        ULONG total;

        total = DiffFile_NumLines(DiffObject_Left(dvd->dvd_DiffObj));
        if (total > dvd->dvd_LeftPane.dpl_MaxLines)
            dvd->dvd_ViewOffsetTop = total - dvd->dvd_LeftPane.dpl_MaxLines;
        else
            dvd->dvd_ViewOffsetTop = 0;
        DV_ClampView(dvd);
        DV_SyncScrollers(cb, dvd);
        redraw = 1;
    }

    if (redraw)
    {
        DV_Refresh(cb, gad, gpi);
        return GMR_REUSE;
    }

    return GMR_REUSE;
}

static ULONG ASM DiffViewDispatcher (
    REG (a0) Class *cl,
    REG (a2) Object *o,
    REG (a1) Msg msg)
{
    struct Gadget *gad;

    gad = (struct Gadget *) o;

    switch (msg->MethodID)
    {
        case OM_NEW:
            return DV_OMNew (cl, gad, (struct opSet *) msg);
        case OM_DISPOSE:
            DV_OMDispose (cl, gad, msg);
            break;
        case OM_SET:
        case OM_UPDATE:
            return DV_OMSet (cl, gad, (struct opSet *) msg);
        case OM_GET:
            return DV_OMGet (cl, gad, (struct opGet *) msg);
        case GM_LAYOUT:
            return DV_GMLayout (cl, gad, (struct gpLayout *) msg);
        case GM_RENDER:
            return DV_GMRender (cl, gad, (struct gpRender *) msg);
        case GM_GOACTIVE:
            return DV_GMGoActive (cl, gad, (struct gpInput *) msg);
        case GM_HANDLEINPUT:
            return DV_GMHandleInput (cl, gad, (struct gpInput *) msg);
        case GM_GOINACTIVE:
            return DV_GMGoinActive (cl, gad, (struct gpInput *) msg);
        default:
            break;
    }

    return DoSuperMethodA (cl, o, msg);
}

static ULONG DV_OMNew (Class *cl, struct Gadget *gad, struct opSet *ops)
{
    struct DiffViewClassBase *cb;
    struct DiffViewData *dvd;
    struct TagItem *tg;
    ULONG disposeMsg;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    if (!cb)
        return 0;

    /* Pass the full opSet to gadgetclass (PianoRoll / vector pattern). */
    gad = (struct Gadget *) DoSuperMethodA (cl, (Object *) gad, (Msg) ops);
    if (!gad)
        return 0;

    dvd = DV_Data (cl, gad);
    if (!dvd)
    {
        disposeMsg = OM_DISPOSE;
        CoerceMethodA (cl, (Object *) gad, (Msg) &disposeMsg);
        return 0;
    }

    dvd->dvd_TabSize = 8;
    dvd->dvd_CurrentLine = 1;
    dvd->dvd_SelMode = DV_SEL_NONE;
    DiffSelect_Init(&dvd->dvd_LeftSelect);
    DiffSelect_Init(&dvd->dvd_RightSelect);
    DiffColors_Default(&dvd->dvd_Colors);

    for (tg = ops->ops_AttrList; tg->ti_Tag != TAG_END; tg++)
    {
        if (tg->ti_Tag == TAG_SKIP)
            tg += tg->ti_Data;
        else
            DV_ApplyTag (cb, dvd, tg->ti_Tag, tg->ti_Data);
    }

    DV_ComputeLayout (gad, dvd);
    DV_ClampView (dvd);
    DV_SyncScrollers (cb, dvd);
    DV_UpdateLineCmp (cb, dvd);

    return (ULONG) gad;
}

static void DV_OMDispose (Class *cl, struct Gadget *gad, Msg msg)
{
    struct DiffViewClassBase *cb;
    struct DiffViewData *dvd;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    dvd = DV_Data (cl, gad);
    if (dvd)
    {
        dvd->dvd_DiffObj = NULL;
        dvd->dvd_LineCmp = NULL;
        dvd->dvd_VertScroller = NULL;
        dvd->dvd_HorizScroller = NULL;
        dvd->dvd_Screen = NULL;
        dvd->dvd_Font = NULL;
        dvd->dvd_SelMode = DV_SEL_NONE;
        DiffSelect_Clear(&dvd->dvd_LeftSelect);
        DiffSelect_Clear(&dvd->dvd_RightSelect);
        if (cb && dvd->dvd_PensReady)
        {
            DiffPens_Free(cb, &dvd->dvd_Pens);
            dvd->dvd_PensReady = 0;
        }
    }

    DoSuperMethodA (cl, (Object *) gad, msg);
}

static ULONG DV_OMSet (Class *cl, struct Gadget *gad, struct opSet *ops)
{
    struct DiffViewClassBase *cb;
    struct DiffViewData *dvd;
    struct TagItem *tg;
    ULONG redraw;
    ULONG geom;
    ULONG pulled;
    ULONG superRet;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    dvd = DV_Data (cl, gad);
    redraw = 0;
    geom = 0;
    if (!cb || !dvd)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) ops);

    for (tg = ops->ops_AttrList; tg->ti_Tag != TAG_END; tg++)
    {
        if (tg->ti_Tag == TAG_SKIP)
            tg += tg->ti_Data;
        else
        {
            if (tg->ti_Tag == GA_Left || tg->ti_Tag == GA_Top ||
                tg->ti_Tag == GA_Width || tg->ti_Tag == GA_Height)
                geom = 1;
            redraw |= DV_ApplyTag (cb, dvd, tg->ti_Tag, tg->ti_Data);
        }
    }

    superRet = DoSuperMethodA (cl, (Object *) gad, (Msg) ops);

    pulled = 0;
    if (cb && dvd)
        pulled = DV_PullFromScrollers (cb, dvd);

    if (redraw || geom || pulled)
    {
        DV_ComputeLayout (gad, dvd);
        DV_ClampView (dvd);
        DV_SyncScrollers (cb, dvd);
        DV_UpdateLineCmp (cb, dvd);
    }

    if ((redraw || pulled) && ops->ops_GInfo && ops->ops_GInfo->gi_Window)
        RefreshGadgets (gad, ops->ops_GInfo->gi_Window, NULL);

    if (redraw || geom || pulled)
        return 1;
    return superRet;
}

static ULONG DV_OMGet (Class *cl, struct Gadget *gad, struct opGet *opg)
{
    struct DiffViewData *dvd;

    dvd = DV_Data (cl, gad);
    switch (opg->opg_AttrID)
    {
        case DIFFVIEW_CurrentLine:
            *(ULONG *) opg->opg_Storage = dvd->dvd_CurrentLine;
            return 1;
        case DIFFVIEW_DiffObject:
            *(APTR *) opg->opg_Storage = dvd->dvd_DiffObj;
            return 1;
        case DIFFVIEW_TabSize:
            *(ULONG *) opg->opg_Storage = dvd->dvd_TabSize;
            return 1;
        case DIFFVIEW_ViewOffsetTop:
            *(ULONG *) opg->opg_Storage = dvd->dvd_ViewOffsetTop;
            return 1;
        case DIFFVIEW_ViewOffsetLeft:
            *(ULONG *) opg->opg_Storage = dvd->dvd_ViewOffsetLeft;
            return 1;
        default:
            break;
    }
    return DoSuperMethodA (cl, (Object *) gad, (Msg) opg);
}

static ULONG DV_GMLayout (Class *cl, struct Gadget *gad, struct gpLayout *gpl)
{
    struct DiffViewData *dvd;
    ULONG rc;

    rc = DoSuperMethodA (cl, (Object *) gad, (Msg) gpl);
    dvd = DV_Data (cl, gad);
    if (dvd)
        DV_ComputeLayout (gad, dvd);
    return rc;
}

static ULONG DV_GMRender (Class *cl, struct Gadget *gad, struct gpRender *gpr)
{
    struct DiffViewClassBase *cb;
    struct DiffViewData *dvd;
    struct RastPort *rp;
    DiffFile *left;
    DiffFile *right;
    struct TextFont *font;
    DiffViewStats stats;
    WORD statusTop;

    if (!gpr || !gpr->gpr_RPort)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    dvd = DV_Data (cl, gad);
    if (!cb || !dvd)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);

    DV_T(cb, "GM_RENDER enter");
    rp = gpr->gpr_RPort;

    if (dvd->dvd_Screen)
        DV_EnsurePens (cb, dvd, dvd->dvd_Screen);
    else if (gpr->gpr_GInfo && gpr->gpr_GInfo->gi_Screen)
        DV_EnsurePens (cb, dvd, gpr->gpr_GInfo->gi_Screen);

    if (!dvd->dvd_PensReady)
    {
        DV_T(cb, "GM_RENDER no pens");
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);
    }

    DV_ComputeLayout (gad, dvd);
    DV_PullFromScrollers(cb, dvd);
    DV_ClampView (dvd);
    DV_SyncScrollers(cb, dvd);
    font = dvd->dvd_Font ? dvd->dvd_Font : rp->Font;
    if (font)
        SetFont (rp, font);

    DiffPens_FillRect(cb, rp, &dvd->dvd_Pens,
                      gad->LeftEdge, gad->TopEdge,
                      gad->Width, gad->Height,
                      0, dvd->dvd_Pens.dp_PenBack);

    if (!dvd->dvd_DiffObj)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);

    left = DiffObject_Left(dvd->dvd_DiffObj);
    right = DiffObject_Right(dvd->dvd_DiffObj);

    DV_T(cb, "GM_RENDER header");
    DiffRender_DrawHeader(cb, rp, &dvd->dvd_Pens,
                          DiffObject_OldName(dvd->dvd_DiffObj),
                          DiffObject_NewName(dvd->dvd_DiffObj),
                          (WORD) (gad->LeftEdge + DV_BORDER),
                          (WORD) (gad->TopEdge + 2),
                          (WORD) (gad->Width - DV_BORDER * 2),
                          DV_HEADER_H, font);

    DV_T(cb, "GM_RENDER overview");
    DiffRender_DrawOverview(cb, rp, &dvd->dvd_Pens, left, right,
                            (WORD) (gad->LeftEdge + DV_BORDER),
                            (WORD) (gad->TopEdge + DV_HEADER_H + 2),
                            (WORD) (gad->Width - DV_BORDER * 2),
                            DV_OVERVIEW_H,
                            dvd->dvd_ViewOffsetTop,
                            dvd->dvd_LeftPane.dpl_MaxLines);

    DV_T(cb, "GM_RENDER panes");
    DiffRender_DrawPane(cb, rp, &dvd->dvd_Pens, left, &dvd->dvd_LeftPane,
                        &dvd->dvd_LeftSelect, dvd->dvd_CurrentLine);
    DiffRender_DrawPane(cb, rp, &dvd->dvd_Pens, right, &dvd->dvd_RightPane,
                        &dvd->dvd_RightSelect, dvd->dvd_CurrentLine);

    statusTop = (WORD) (gad->TopEdge + gad->Height - DV_STATUS_H - 2);
    DiffRender_CountStats(left, right, &stats);
    DiffRender_DrawStatusBar(cb, rp, &dvd->dvd_Pens,
                             (WORD) (gad->LeftEdge + DV_BORDER),
                             statusTop,
                             (WORD) (gad->Width - DV_BORDER * 2),
                             DV_STATUS_H, font, &stats);

    DV_T(cb, "GM_RENDER done");
    return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);
}

static ULONG DV_GMGoActive (Class *cl, struct Gadget *gad, struct gpInput *gpi)
{
    /* Read-only viewer: do not enter GMR_MEACTIVE here.  Selection drag
     * returns GMR_MEACTIVE from GM_HANDLEINPUT on SELECTDOWN only. */
    (void) DoSuperMethodA (cl, (Object *) gad, (Msg) gpi);
    return GMR_NOREUSE;
}

static ULONG DV_GMHandleInput (Class *cl, struct Gadget *gad, struct gpInput *gpi)
{
    struct DiffViewClassBase *cb;
    struct DiffViewData *dvd;
    struct InputEvent *ie;
    ULONG rc;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    dvd = DV_Data (cl, gad);
    ie = gpi ? gpi->gpi_IEvent : NULL;

    if (!cb || !dvd)
        return GMR_REUSE;

    DV_PullFromScrollers(cb, dvd);

    /* Intuition tick with updated gpi_Mouse while we hold GMR_MEACTIVE. */
    if (!ie)
    {
        if (dvd->dvd_SelMode == DV_SEL_LEFT_STARTED)
        {
            DV_UpdateSelectionDrag(cb, gad, dvd, gpi, 0);
            return GMR_MEACTIVE;
        }
        if (dvd->dvd_SelMode == DV_SEL_RIGHT_STARTED)
        {
            DV_UpdateSelectionDrag(cb, gad, dvd, gpi, 1);
            return GMR_MEACTIVE;
        }
        return GMR_REUSE;
    }

    if (ie->ie_Class == IECLASS_RAWMOUSE)
    {
        if (ie->ie_Code == SELECTDOWN)
            return DV_HandleSelectDown(cb, gad, dvd, gpi);
        if (ie->ie_Code == SELECTUP)
            return DV_HandleSelectUp(dvd);

        if (dvd->dvd_SelMode == DV_SEL_LEFT_STARTED)
        {
            DV_UpdateSelectionDrag(cb, gad, dvd, gpi, 0);
            return GMR_MEACTIVE;
        }
        if (dvd->dvd_SelMode == DV_SEL_RIGHT_STARTED)
        {
            DV_UpdateSelectionDrag(cb, gad, dvd, gpi, 1);
            return GMR_MEACTIVE;
        }
    }

    rc = DV_HandleKey(cb, gad, dvd, gpi, ie);
    return rc;
}

static ULONG DV_GMGoinActive (Class *cl, struct Gadget *gad, struct gpInput *gpi)
{
    struct DiffViewData *dvd;
    ULONG rc;

    rc = DoSuperMethodA (cl, (Object *) gad, (Msg) gpi);
    dvd = DV_Data (cl, gad);
    if (dvd)
        DV_HandleSelectUp(dvd);
    return rc;
}

static BOOL DV_CopySelection (struct DiffViewClassBase *cb,
                              struct DiffViewData *dvd)
{
    DiffSelect *sel;
    DiffFile *file;
    ULONG leftChars;
    ULONG rightChars;

    if (!cb || !dvd || !dvd->dvd_DiffObj)
        return FALSE;

    leftChars = DiffSelect_GetTotalChars(&dvd->dvd_LeftSelect,
                                         DiffObject_Left(dvd->dvd_DiffObj));
    rightChars = DiffSelect_GetTotalChars(&dvd->dvd_RightSelect,
                                          DiffObject_Right(dvd->dvd_DiffObj));

    sel = NULL;
    file = NULL;
    if (leftChars > 0 && leftChars >= rightChars)
    {
        sel = &dvd->dvd_LeftSelect;
        file = DiffObject_Left(dvd->dvd_DiffObj);
    }
    else if (rightChars > 0)
    {
        sel = &dvd->dvd_RightSelect;
        file = DiffObject_Right(dvd->dvd_DiffObj);
    }

    if (!sel || !file)
        return FALSE;

    return DiffClip_CopySelection(cb, sel, file);
}

static ULONG DV_HandleOverviewClick (struct DiffViewClassBase *cb,
                                     struct Gadget *gad,
                                     struct DiffViewData *dvd,
                                     struct gpInput *gpi)
{
    WORD ox;
    WORD oy;
    WORD ow;
    WORD oh;
    ULONG numLines;
    ULONG lineId;
    DiffFile *left;

    if (!cb || !gad || !dvd || !gpi || !dvd->dvd_DiffObj)
        return 0;

    ox = (WORD) (gad->LeftEdge + DV_BORDER);
    oy = (WORD) (gad->TopEdge + DV_HEADER_H + 2);
    ow = (WORD) (gad->Width - DV_BORDER * 2);
    oh = DV_OVERVIEW_H;

    if (gpi->gpi_Mouse.X < ox || gpi->gpi_Mouse.X >= ox + ow)
        return 0;
    if (gpi->gpi_Mouse.Y < oy || gpi->gpi_Mouse.Y >= oy + oh)
        return 0;

    left = DiffObject_Left(dvd->dvd_DiffObj);
    numLines = DiffFile_NumLines(left);
    if (numLines < 1)
        numLines = 1;

    lineId = DiffRender_OverviewLineAt(gpi->gpi_Mouse.Y, oy, oh, numLines);

    if (lineId + 1 > dvd->dvd_ViewOffsetTop + dvd->dvd_LeftPane.dpl_MaxLines)
    {
        if (lineId + 1 >= dvd->dvd_LeftPane.dpl_MaxLines)
            dvd->dvd_ViewOffsetTop = lineId + 1 - dvd->dvd_LeftPane.dpl_MaxLines;
        else
            dvd->dvd_ViewOffsetTop = 0;
    }
    else if (lineId < dvd->dvd_ViewOffsetTop)
        dvd->dvd_ViewOffsetTop = lineId;

    DV_ClampView(dvd);
    DV_SetCurrentLine(cb, dvd, lineId + 1);
    DV_SyncScrollers(cb, dvd);
    DV_Refresh(cb, gad, gpi);
    return 1;
}

Class *DiffViewCreateClass (struct DiffViewClassBase *cb)
{
    Class *cl;

    cl = MakeClass(DIFFVIEWCLASS, GADGETCLASS, NULL,
                   (ULONG) sizeof(struct DiffViewData), 0);
    if (cl)
    {
        cl->cl_UserData = (ULONG) cb;
        cl->cl_Dispatcher.h_Entry = (ULONG (*)()) DiffViewDispatcher;
        AddClass(cl);
    }
    return cl;
}

void   DiffViewDeleteClass (struct DiffViewClassBase *cb, Class *cl)
{
    if (!cl)
        return;
    RemoveClass(cl);
    if (!FreeClass(cl))
    {
        AddClass(cl);
        (void) cb;
    }
}
