/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * LineCmpGClass.c - linecmp.gadget BOOPSI class.
 *
 * Shows old and new versions of a single line one above the other.
 * Normally driven automatically by an associated diffview.gadget.
 */

#include "classbase.h"
#include "diffpens.h"
#include <gadgets/scroller.h>

static void LC_SyncScroller(struct DiffViewClassBase *cb,
                            struct LineCmpData *lcd, struct Gadget *gad);
static ULONG LC_PullFromScroller(struct DiffViewClassBase *cb,
                                 struct LineCmpData *lcd);

#define HEADER_H    2
#define LINE_GAP    2

struct LineCmpData
{
    struct Screen    *lcd_Screen;
    struct TextFont  *lcd_Font;
    Object           *lcd_HorizScroller;
    STRPTR            lcd_OldLine;
    STRPTR            lcd_NewLine;
    ULONG             lcd_ViewOffsetLeft;
    ULONG             lcd_TabSize;
    DiffPens          lcd_Pens;
    DiffColors        lcd_Colors;
    UBYTE             lcd_PensReady;
};

static ULONG ASM LineCmpDispatcher (
    REG (a0) Class *cl,
    REG (a2) Object *o,
    REG (a1) Msg msg);

static ULONG LC_OMNew (Class *cl, struct Gadget *gad, struct opSet *ops);
static void  LC_OMDispose (Class *cl, struct Gadget *gad, Msg msg);
static ULONG LC_OMSet (Class *cl, struct Gadget *gad, struct opSet *ops);
static ULONG LC_OMGet (Class *cl, struct Gadget *gad, struct opGet *opg);
static ULONG LC_GMLayout (Class *cl, struct Gadget *gad, struct gpLayout *gpl);
static ULONG LC_GMRender (Class *cl, struct Gadget *gad, struct gpRender *gpr);
static ULONG LC_GMHandleInput (Class *cl, struct Gadget *gad, struct gpInput *gpi);
static ULONG LC_GMDomain (Class *cl, struct Gadget *gad, struct gpDomain *gpd);

static struct LineCmpData *LC_Data (Class *cl, struct Gadget *gad);
static ULONG LC_ApplyTag (struct DiffViewClassBase *cb,
                          struct LineCmpData *lcd, ULONG tag, ULONG data);
static void  LC_EnsurePens (struct DiffViewClassBase *cb,
                            struct LineCmpData *lcd, struct Screen *scr);

Class *LineCmpCreateClass (struct DiffViewClassBase *cb);
void   LineCmpDeleteClass (struct DiffViewClassBase *cb, Class *cl);

static struct LineCmpData *LC_Data (Class *cl, struct Gadget *gad)
{
    return (struct LineCmpData *) INST_DATA (cl, gad);
}

static void LC_SyncScroller(struct DiffViewClassBase *cb,
                            struct LineCmpData *lcd, struct Gadget *gad)
{
    ULONG maxCol;
    ULONG vis;

    if (!cb || !lcd || !lcd->lcd_HorizScroller || !gad)
        return;

    maxCol = 0;
    if (lcd->lcd_OldLine && strlen(lcd->lcd_OldLine) > maxCol)
        maxCol = (ULONG) strlen(lcd->lcd_OldLine);
    if (lcd->lcd_NewLine && strlen(lcd->lcd_NewLine) > maxCol)
        maxCol = (ULONG) strlen(lcd->lcd_NewLine);

    vis = (ULONG) (gad->Width / 8);
    if (vis < 1)
        vis = 1;

    SetGadgetAttrs((struct Gadget *) lcd->lcd_HorizScroller, NULL, NULL,
                   SCROLLER_Top, (WORD) lcd->lcd_ViewOffsetLeft,
                   SCROLLER_Visible, (WORD) vis,
                   SCROLLER_Total, (WORD) maxCol,
                   TAG_END);
}

static ULONG LC_PullFromScroller(struct DiffViewClassBase *cb,
                                 struct LineCmpData *lcd)
{
    ULONG top;

    if (!cb || !lcd || !lcd->lcd_HorizScroller)
        return 0;

    top = 0;
    GetAttr(SCROLLER_Top, (APTR) lcd->lcd_HorizScroller, &top);
    if (top != lcd->lcd_ViewOffsetLeft)
    {
        lcd->lcd_ViewOffsetLeft = top;
        return 1;
    }
    return 0;
}

static ULONG LC_ApplyTag (struct DiffViewClassBase *cb,
                          struct LineCmpData *lcd, ULONG tag, ULONG data)
{
    ULONG redraw;

    redraw = 0;
    if (!lcd)
        return 0;

    switch (tag)
    {
        case LINECMP_Font:
            lcd->lcd_Font = (struct TextFont *) data;
            redraw = 1;
            break;
        case LINECMP_HorizScroller:
            lcd->lcd_HorizScroller = (Object *) data;
            break;
        case LINECMP_Screen:
            lcd->lcd_Screen = (struct Screen *) data;
            if (lcd->lcd_PensReady)
            {
                DiffPens_Free(cb, &lcd->lcd_Pens);
                lcd->lcd_PensReady = 0;
            }
            redraw = 1;
            break;
        case LINECMP_TabSize:
            lcd->lcd_TabSize = data ? data : 8;
            redraw = 1;
            break;
        case LINECMP_ViewOffsetLeft:
            lcd->lcd_ViewOffsetLeft = data;
            redraw = 1;
            break;
        case LINECMP_OldLine:
            lcd->lcd_OldLine = (STRPTR) data;
            redraw = 1;
            break;
        case LINECMP_NewLine:
            lcd->lcd_NewLine = (STRPTR) data;
            redraw = 1;
            break;
        default:
            break;
    }
    return redraw;
}

static void LC_EnsurePens (struct DiffViewClassBase *cb,
                           struct LineCmpData *lcd, struct Screen *scr)
{
    if (!cb || !lcd || lcd->lcd_PensReady || !scr)
        return;

    DiffColors_Default(&lcd->lcd_Colors);
    if (DiffPens_Init(cb, &lcd->lcd_Pens, scr, &lcd->lcd_Colors))
        lcd->lcd_PensReady = 1;
}

static ULONG ASM LineCmpDispatcher (
    REG (a0) Class *cl,
    REG (a2) Object *o,
    REG (a1) Msg msg)
{
    struct Gadget *gad;
    ULONG rc;

    gad = (struct Gadget *) o;
    rc = 0;

    switch (msg->MethodID)
    {
        case OM_NEW:
            return LC_OMNew (cl, gad, (struct opSet *) msg);
        case OM_DISPOSE:
            LC_OMDispose (cl, gad, msg);
            break;
        case OM_SET:
        case OM_UPDATE:
            return LC_OMSet (cl, gad, (struct opSet *) msg);
        case OM_GET:
            return LC_OMGet (cl, gad, (struct opGet *) msg);
        case GM_LAYOUT:
            return LC_GMLayout (cl, gad, (struct gpLayout *) msg);
        case GM_RENDER:
            return LC_GMRender (cl, gad, (struct gpRender *) msg);
        case GM_HANDLEINPUT:
            return LC_GMHandleInput (cl, gad, (struct gpInput *) msg);
        case GM_DOMAIN:
            return LC_GMDomain (cl, gad, (struct gpDomain *) msg);
        default:
            break;
    }

    return DoSuperMethodA (cl, o, msg);
}

static ULONG LC_OMNew (Class *cl, struct Gadget *gad, struct opSet *ops)
{
    struct DiffViewClassBase *cb;
    struct LineCmpData *lcd;
    struct TagItem *tg;
    ULONG disposeMsg;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    if (!cb)
        return 0;

    gad = (struct Gadget *) DoSuperMethodA (cl, (Object *) gad, (Msg) ops);
    if (!gad)
        return 0;

    lcd = LC_Data (cl, gad);
    if (!lcd)
    {
        disposeMsg = OM_DISPOSE;
        CoerceMethodA (cl, (Object *) gad, (Msg) &disposeMsg);
        return 0;
    }

    lcd->lcd_TabSize = 8;
    lcd->lcd_ViewOffsetLeft = 0;
    DiffColors_Default(&lcd->lcd_Colors);

    for (tg = ops->ops_AttrList; tg->ti_Tag != TAG_END; tg++)
    {
        if (tg->ti_Tag == TAG_SKIP)
            tg += tg->ti_Data;
        else
            LC_ApplyTag (cb, lcd, tg->ti_Tag, tg->ti_Data);
    }

    return (ULONG) gad;
}

static void LC_OMDispose (Class *cl, struct Gadget *gad, Msg msg)
{
    struct DiffViewClassBase *cb;
    struct LineCmpData *lcd;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    lcd = LC_Data (cl, gad);
    if (lcd)
    {
        lcd->lcd_OldLine = NULL;
        lcd->lcd_NewLine = NULL;
        lcd->lcd_HorizScroller = NULL;
        lcd->lcd_Screen = NULL;
        lcd->lcd_Font = NULL;
        if (cb && lcd->lcd_PensReady)
        {
            DiffPens_Free(cb, &lcd->lcd_Pens);
            lcd->lcd_PensReady = 0;
        }
    }

    DoSuperMethodA (cl, (Object *) gad, msg);
}

static ULONG LC_OMSet (Class *cl, struct Gadget *gad, struct opSet *ops)
{
    struct DiffViewClassBase *cb;
    struct LineCmpData *lcd;
    struct TagItem *tg;
    ULONG redraw;
    ULONG geom;
    ULONG pulled;
    ULONG superRet;

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    lcd = LC_Data (cl, gad);
    redraw = 0;
    geom = 0;
    if (!cb || !lcd)
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
            redraw |= LC_ApplyTag (cb, lcd, tg->ti_Tag, tg->ti_Data);
        }
    }

    superRet = DoSuperMethodA (cl, (Object *) gad, (Msg) ops);

    pulled = 0;
    if (lcd)
        pulled = LC_PullFromScroller (cb, lcd);

    if (redraw || geom || pulled)
        LC_SyncScroller (cb, lcd, gad);

    if ((redraw || pulled) && ops->ops_GInfo && ops->ops_GInfo->gi_Window)
        RefreshGadgets ((struct Gadget *) gad, ops->ops_GInfo->gi_Window, NULL);

    if (redraw || geom || pulled)
        return 1;
    return superRet;
}

static ULONG LC_OMGet (Class *cl, struct Gadget *gad, struct opGet *opg)
{
    struct LineCmpData *lcd;

    lcd = LC_Data (cl, gad);
    switch (opg->opg_AttrID)
    {
        case LINECMP_TabSize:
            *(ULONG *) opg->opg_Storage = lcd->lcd_TabSize;
            return 1;
        case LINECMP_ViewOffsetLeft:
            *(ULONG *) opg->opg_Storage = lcd->lcd_ViewOffsetLeft;
            return 1;
        default:
            break;
    }
    return DoSuperMethodA (cl, (Object *) gad, (Msg) opg);
}

static ULONG LC_GMLayout (Class *cl, struct Gadget *gad, struct gpLayout *gpl)
{
    return DoSuperMethodA (cl, (Object *) gad, (Msg) gpl);
}

static ULONG LC_GMRender (Class *cl, struct Gadget *gad, struct gpRender *gpr)
{
    struct DiffViewClassBase *cb;
    struct LineCmpData *lcd;
    struct RastPort *rp;
    struct TextFont *font;
    WORD fontH;
    WORD half;
    WORD yOld;
    WORD yNew;

    if (!gpr || !gpr->gpr_RPort)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);

    cb = (struct DiffViewClassBase *) cl->cl_UserData;
    lcd = LC_Data (cl, gad);
    if (!cb || !lcd)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);

    rp = gpr->gpr_RPort;
    if (lcd->lcd_Screen)
        LC_EnsurePens (cb, lcd, lcd->lcd_Screen);
    else if (gpr->gpr_GInfo && gpr->gpr_GInfo->gi_Screen)
        LC_EnsurePens (cb, lcd, gpr->gpr_GInfo->gi_Screen);

    if (!lcd->lcd_PensReady)
        return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);

    LC_PullFromScroller(cb, lcd);
    LC_SyncScroller(cb, lcd, gad);

    font = lcd->lcd_Font ? lcd->lcd_Font : rp->Font;
    fontH = font ? font->tf_YSize : 8;
    half = (gad->Height - HEADER_H) / 2;

    DiffPens_FillRect(cb, rp, &lcd->lcd_Pens,
                      gad->LeftEdge, gad->TopEdge,
                      gad->Width, gad->Height,
                      0, lcd->lcd_Pens.dp_PenBack);

    yOld = (WORD) (gad->TopEdge + 2);
    yNew = (WORD) (gad->TopEdge + half + LINE_GAP);
    if (font)
        SetFont(rp, font);

    if (lcd->lcd_OldLine && lcd->lcd_OldLine[0] && font)
    {
        DiffPens_SetJam2(cb, rp, lcd->lcd_Pens.dp_PenText, lcd->lcd_Pens.dp_PenRed);
        Move(rp, gad->LeftEdge + 4, yOld + font->tf_Baseline);
        Text(rp, lcd->lcd_OldLine + lcd->lcd_ViewOffsetLeft,
             (LONG) strlen(lcd->lcd_OldLine + lcd->lcd_ViewOffsetLeft));
    }

    if (lcd->lcd_NewLine && lcd->lcd_NewLine[0] && font)
    {
        DiffPens_SetJam2(cb, rp, lcd->lcd_Pens.dp_PenText, lcd->lcd_Pens.dp_PenGreen);
        Move(rp, gad->LeftEdge + 4, yNew + font->tf_Baseline);
        Text(rp, lcd->lcd_NewLine + lcd->lcd_ViewOffsetLeft,
             (LONG) strlen(lcd->lcd_NewLine + lcd->lcd_ViewOffsetLeft));
    }

    return DoSuperMethodA (cl, (Object *) gad, (Msg) gpr);
}

static ULONG LC_GMHandleInput (Class *cl, struct Gadget *gad, struct gpInput *gpi)
{
    (void) cl;
    (void) gad;
    (void) gpi;
    return GMR_REUSE;
}

static ULONG LC_GMDomain (Class *cl, struct Gadget *gad, struct gpDomain *gpd)
{
    struct TextFont *font;
    WORD fontH;
    ULONG rc;

    rc = DoSuperMethodA (cl, (Object *) gad, (Msg) gpd);
    if (gpd && gpd->gpd_RPort && gpd->gpd_Which == GDOMAIN_MINIMUM)
    {
        font = gpd->gpd_RPort->Font;
        fontH = font ? font->tf_YSize : 8;
        gpd->gpd_Domain.Width = 80;
        gpd->gpd_Domain.Height = (WORD) (fontH * 2 + HEADER_H + LINE_GAP);
    }
    return rc;
}

Class *LineCmpCreateClass (struct DiffViewClassBase *cb)
{
    Class *cl;

    cl = MakeClass(LINECMPCLASS, GADGETCLASS, NULL,
                   (ULONG) sizeof(struct LineCmpData), 0);
    if (cl)
    {
        cl->cl_UserData = (ULONG) cb;
        cl->cl_Dispatcher.h_Entry = (ULONG (*)()) LineCmpDispatcher;
        AddClass(cl);
    }
    return cl;
}

void LineCmpDeleteClass (struct DiffViewClassBase *cb, Class *cl)
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
