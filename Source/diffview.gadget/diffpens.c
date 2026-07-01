/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 *
 * diffpens.c - colour and pen management for diffview.gadget.
 *
 * DIFFVIEW_*Col tag RGB values are mapped with ObtainBestPen on indexed
 * screens (OS 3.x build).  One DrawInfo set per screen is shared between
 * diffview + linecmp gadgets (ref counted).
 */

#include "classbase.h"
#include "diffpens.h"
#include "diffline.h"

#include <graphics/view.h>
#include <graphics/gfxmacros.h>

#ifndef OS30_ONLY
#include <graphics/rpattr.h>
#include <utility/tagitem.h>
#endif

#define DP_RGBEXP(b) \
    (((ULONG)(b) << 24) | ((ULONG)(b) << 16) | ((ULONG)(b) << 8) | (ULONG)(b))

void DiffColors_Default(DiffColors *colors)
{
    if (!colors)
        return;

    colors->dc_BackCol           = 0x00FFFFFFUL;
    colors->dc_TextCol           = 0x00000000UL;
    colors->dc_OldCol            = 0x00FFCCCCUL;
    colors->dc_NewCol            = 0x00CCFFCCUL;
    colors->dc_BlankCol          = 0x00DDDDDDUL;
    colors->dc_LineNoBackCol     = 0x00EEEEEEUL;
    colors->dc_LineNoTextCol     = 0x00444444UL;
    colors->dc_GeneralBackCol    = 0x00CCCCCCUL;
    colors->dc_GeneralShineCol   = 0x00FFFFFFUL;
    colors->dc_GeneralShadowCol  = 0x00888888UL;
    colors->dc_OverviewOldCol    = 0x00FF6666UL;
    colors->dc_OverviewNewCol    = 0x0066FF66UL;
    colors->dc_WsDifferenceCol   = 0x00FFFFCCUL;
    colors->dc_OverviewWsCol     = 0x00CCCC66UL;
    colors->dc_CurrentLineCol    = 0x000000FFUL;
    colors->dc_Flags             = 0;
}

void DiffPens_SetJam2(struct DiffViewClassBase *cb, struct RastPort *rp,
                      LONG apen, LONG bpen)
{
    if (!rp || !cb)
        return;
    if (apen < 0)
        apen = 1;
    if (bpen < 0)
        bpen = apen;
    SetAPen(rp, apen);
    SetBPen(rp, bpen);
    SetDrMd(rp, JAM2);
}

LONG DiffPens_LineStateBPen(DiffPens *pens, UBYTE state)
{
    if (!pens)
        return 1;

    switch (state)
    {
        case DLS_DELETED:
            return pens->dp_PenOld;
        case DLS_ADDED:
            return pens->dp_PenNew;
        case DLS_CHANGED:
            return pens->dp_PenWs;
        case DLS_WS_ONLY:
            return pens->dp_PenWs;
        default:
            return pens->dp_PenBack;
    }
}

static LONG dp_ObtainColorPen(struct DiffViewClassBase *cb, struct ColorMap *cm,
                              ULONG rgb)
{
    ULONG r;
    ULONG g;
    ULONG b;
    LONG pen;

    (void) cb;

    if (!cm)
        return -1;

    r = (rgb >> 16) & 0xFF;
    g = (rgb >> 8) & 0xFF;
    b = rgb & 0xFF;
    pen = (LONG) ObtainBestPen(cm, DP_RGBEXP((UBYTE) r), DP_RGBEXP((UBYTE) g),
                               DP_RGBEXP((UBYTE) b), TAG_DONE);
    return pen;
}

static void dp_InitPenSlots(DiffPens *pens)
{
    pens->dp_PenBack = -1;
    pens->dp_PenText = -1;
    pens->dp_PenHighlight = -1;
    pens->dp_PenRed = -1;
    pens->dp_PenYellow = -1;
    pens->dp_PenGreen = -1;
    pens->dp_PenGray = -1;
    pens->dp_PenOld = -1;
    pens->dp_PenNew = -1;
    pens->dp_PenBlank = -1;
    pens->dp_PenLineNoBack = -1;
    pens->dp_PenLineNoText = -1;
    pens->dp_PenGeneralBack = -1;
    pens->dp_PenGeneralShine = -1;
    pens->dp_PenGeneralShadow = -1;
    pens->dp_PenOverviewOld = -1;
    pens->dp_PenOverviewNew = -1;
    pens->dp_PenWs = -1;
    pens->dp_PenOverviewWs = -1;
    pens->dp_PenCurrent = -1;
}

static void dp_ReleaseGadgetPens(struct DiffViewClassBase *cb,
                                 struct Screen *screen, DiffPens *pens)
{
    struct ColorMap *cm;

    if (!cb || !pens || !pens->dp_OwnsPens || !screen)
        return;

    cm = screen->ViewPort.ColorMap;
    if (!cm)
        return;

    if (pens->dp_PenBack >= 0 && (pens->dp_Colors.dc_Flags & DCF_CUSTOM_BACK))
        ReleasePen(cm, pens->dp_PenBack);
    if (pens->dp_PenText >= 0 && (pens->dp_Colors.dc_Flags & DCF_CUSTOM_TEXT))
        ReleasePen(cm, pens->dp_PenText);
    if (pens->dp_PenOld >= 0)
        ReleasePen(cm, pens->dp_PenOld);
    if (pens->dp_PenNew >= 0)
        ReleasePen(cm, pens->dp_PenNew);
    if (pens->dp_PenBlank >= 0)
        ReleasePen(cm, pens->dp_PenBlank);
    if (pens->dp_PenLineNoBack >= 0)
        ReleasePen(cm, pens->dp_PenLineNoBack);
    if (pens->dp_PenLineNoText >= 0)
        ReleasePen(cm, pens->dp_PenLineNoText);
    if (pens->dp_PenGeneralBack >= 0)
        ReleasePen(cm, pens->dp_PenGeneralBack);
    if (pens->dp_PenGeneralShine >= 0)
        ReleasePen(cm, pens->dp_PenGeneralShine);
    if (pens->dp_PenGeneralShadow >= 0)
        ReleasePen(cm, pens->dp_PenGeneralShadow);
    if (pens->dp_PenOverviewOld >= 0)
        ReleasePen(cm, pens->dp_PenOverviewOld);
    if (pens->dp_PenOverviewNew >= 0)
        ReleasePen(cm, pens->dp_PenOverviewNew);
    if (pens->dp_PenWs >= 0)
        ReleasePen(cm, pens->dp_PenWs);
    if (pens->dp_PenOverviewWs >= 0)
        ReleasePen(cm, pens->dp_PenOverviewWs);
    if (pens->dp_PenCurrent >= 0)
        ReleasePen(cm, pens->dp_PenCurrent);

    dp_InitPenSlots(pens);
    pens->dp_OwnsPens = 0;
}

static void dp_ReleaseSharedPens(struct DiffViewClassBase *cb,
                                 struct DiffViewSharedPens *sp)
{
    if (!cb || !sp || !sp->dvs_Screen)
        return;

    if (sp->dvs_DrawInfo)
    {
        FreeScreenDrawInfo(sp->dvs_Screen, sp->dvs_DrawInfo);
        sp->dvs_DrawInfo = NULL;
    }

    sp->dvs_Screen = NULL;
    sp->dvs_RefCount = 0;
}

void DiffPens_InitLibrary(struct DiffViewClassBase *cb)
{
    struct DiffViewSharedPens *sp;

    if (!cb)
        return;

    sp = &cb->dvb_SharedPens;
    sp->dvs_Screen = NULL;
    sp->dvs_DrawInfo = NULL;
    sp->dvs_RefCount = 0;
}

void DiffPens_ShutdownShared(struct DiffViewClassBase *cb)
{
    struct DiffViewSharedPens *sp;

    if (!cb)
        return;

    sp = &cb->dvb_SharedPens;
    if (!sp->dvs_Screen)
        return;

    sp->dvs_RefCount = 0;
    dp_ReleaseSharedPens(cb, sp);
}

static BOOL dp_AcquireShared(struct DiffViewClassBase *cb, struct Screen *screen)
{
    struct DiffViewSharedPens *sp;
    struct DrawInfo *di;

    if (!cb || !screen)
        return FALSE;

    sp = &cb->dvb_SharedPens;
    if (sp->dvs_Screen == screen && sp->dvs_DrawInfo)
    {
        sp->dvs_RefCount++;
        return TRUE;
    }

    if (sp->dvs_RefCount > 0)
        dp_ReleaseSharedPens(cb, sp);

    di = GetScreenDrawInfo(screen);
    if (!di || !di->dri_Pens)
        return FALSE;

    sp->dvs_Screen = screen;
    sp->dvs_DrawInfo = di;
    sp->dvs_RefCount = 1;
    return TRUE;
}

static void dp_ReleaseShared(struct DiffViewClassBase *cb)
{
    struct DiffViewSharedPens *sp;

    if (!cb)
        return;

    sp = &cb->dvb_SharedPens;
    if (sp->dvs_RefCount == 0)
        return;

    sp->dvs_RefCount--;
    if (sp->dvs_RefCount == 0)
        dp_ReleaseSharedPens(cb, sp);
}

static void dp_BuildGadgetPens(struct DiffViewClassBase *cb, DiffPens *pens,
                               struct Screen *screen, DiffColors *colors)
{
    struct ColorMap *cm;
    struct DrawInfo *di;

    dp_InitPenSlots(pens);
    pens->dp_Screen = screen;
    pens->dp_Colors = *colors;
    pens->dp_DirectRGB = 0;
    pens->dp_Shared = 1;
    pens->dp_OwnsPens = 0;

    di = cb->dvb_SharedPens.dvs_DrawInfo;
    cm = screen->ViewPort.ColorMap;

#ifndef OS30_ONLY
    if (screen->ViewPort.Depth >= 15)
    {
        pens->dp_DirectRGB = 1;
        return;
    }
#endif

    if (!cm)
        return;

    if (pens->dp_Colors.dc_Flags & DCF_CUSTOM_BACK)
        pens->dp_PenBack = dp_ObtainColorPen(cb, cm, colors->dc_BackCol);
    else if (di)
        pens->dp_PenBack = (LONG) di->dri_Pens[BACKGROUNDPEN];
    else
        pens->dp_PenBack = dp_ObtainColorPen(cb, cm, colors->dc_BackCol);

    if (pens->dp_Colors.dc_Flags & DCF_CUSTOM_TEXT)
        pens->dp_PenText = dp_ObtainColorPen(cb, cm, colors->dc_TextCol);
    else if (di)
        pens->dp_PenText = (LONG) di->dri_Pens[TEXTPEN];
    else
        pens->dp_PenText = dp_ObtainColorPen(cb, cm, colors->dc_TextCol);

    if (di)
        pens->dp_PenHighlight = (LONG) di->dri_Pens[HIGHLIGHTTEXTPEN];
    pens->dp_PenOld = dp_ObtainColorPen(cb, cm, colors->dc_OldCol);
    pens->dp_PenNew = dp_ObtainColorPen(cb, cm, colors->dc_NewCol);
    pens->dp_PenBlank = dp_ObtainColorPen(cb, cm, colors->dc_BlankCol);
    pens->dp_PenLineNoBack = dp_ObtainColorPen(cb, cm, colors->dc_LineNoBackCol);
    pens->dp_PenLineNoText = dp_ObtainColorPen(cb, cm, colors->dc_LineNoTextCol);
    pens->dp_PenGeneralBack = dp_ObtainColorPen(cb, cm, colors->dc_GeneralBackCol);
    pens->dp_PenGeneralShine = dp_ObtainColorPen(cb, cm, colors->dc_GeneralShineCol);
    pens->dp_PenGeneralShadow = dp_ObtainColorPen(cb, cm, colors->dc_GeneralShadowCol);
    pens->dp_PenOverviewOld = dp_ObtainColorPen(cb, cm, colors->dc_OverviewOldCol);
    pens->dp_PenOverviewNew = dp_ObtainColorPen(cb, cm, colors->dc_OverviewNewCol);
    pens->dp_PenWs = dp_ObtainColorPen(cb, cm, colors->dc_WsDifferenceCol);
    pens->dp_PenOverviewWs = dp_ObtainColorPen(cb, cm, colors->dc_OverviewWsCol);
    pens->dp_PenCurrent = dp_ObtainColorPen(cb, cm, colors->dc_CurrentLineCol);

    pens->dp_PenRed = pens->dp_PenOld;
    pens->dp_PenGreen = pens->dp_PenNew;
    pens->dp_PenYellow = pens->dp_PenWs;
    if (di)
        pens->dp_PenGray = (LONG) di->dri_Pens[SHINEPEN];
    else
        pens->dp_PenGray = pens->dp_PenLineNoBack;

    pens->dp_OwnsPens = 1;
}

BOOL DiffPens_Init(struct DiffViewClassBase *cb, DiffPens *pens,
                   struct Screen *screen, DiffColors *colors)
{
    if (!cb || !pens || !screen || !colors)
        return FALSE;

    if (!dp_AcquireShared(cb, screen))
        return FALSE;

    dp_BuildGadgetPens(cb, pens, screen, colors);
    return TRUE;
}

void DiffPens_Free(struct DiffViewClassBase *cb, DiffPens *pens)
{
    struct Screen *screen;

    if (!pens)
        return;

    screen = pens->dp_Screen;
    if (pens->dp_OwnsPens)
        dp_ReleaseGadgetPens(cb, screen, pens);

    if (pens->dp_Shared)
    {
        dp_ReleaseShared(cb);
        pens->dp_Shared = 0;
        pens->dp_Screen = NULL;
    }
}

void DiffPens_SetWriteColor(struct DiffViewClassBase *cb, struct RastPort *rp,
                            DiffPens *pens, ULONG rgb, LONG pen)
{
    if (!rp || !pens)
        return;

#ifndef OS30_ONLY
    if (pens->dp_DirectRGB)
    {
        SetRPAttrs(rp, RPTAG_FGColor, rgb, TAG_DONE);
        SetDrMd(rp, JAM2);
        SetAPen(rp, 1);
        return;
    }
#else
    (void) rgb;
#endif

    (void) cb;
    DiffPens_SetJam2(cb, rp, pen, pens->dp_PenBack);
}

void DiffPens_FillRect(struct DiffViewClassBase *cb, struct RastPort *rp,
                       DiffPens *pens, LONG x, LONG y, LONG w, LONG h,
                       ULONG rgb, LONG pen)
{
    if (!rp || !pens || w < 1 || h < 1)
        return;

#ifndef OS30_ONLY
    if (pens->dp_DirectRGB)
    {
        SetRPAttrs(rp, RPTAG_BgColor, rgb, TAG_DONE);
        RectFill(rp, x, y, x + w - 1, y + h - 1);
        return;
    }
#else
    (void) rgb;
#endif

    (void) cb;
    if (pen < 0)
        pen = pens->dp_PenBack;
    SetAPen(rp, pen);
    RectFill(rp, x, y, x + w - 1, y + h - 1);
}

void DiffPens_FillPattern(struct DiffViewClassBase *cb, struct RastPort *rp,
                          DiffPens *pens, LONG x, LONG y, LONG w, LONG h,
                          ULONG rgb, LONG apen, LONG bpen)
{
    UWORD pattern[2];

    if (!rp || !pens || w < 1 || h < 1)
        return;

#ifndef OS30_ONLY
    if (pens->dp_DirectRGB)
    {
        SetRPAttrs(rp, RPTAG_BgColor, rgb, TAG_DONE);
        RectFill(rp, x, y, x + w - 1, y + h - 1);
        return;
    }
#else
    (void) rgb;
#endif

    (void) cb;
    pattern[0] = 0xAAAA;
    pattern[1] = 0x5555;
    DiffPens_SetJam2(cb, rp, apen, bpen);
    SetAfPt(rp, pattern, 2);
    RectFill(rp, x, y, x + w - 1, y + h - 1);
    SetAfPt(rp, NULL, 0);
}
