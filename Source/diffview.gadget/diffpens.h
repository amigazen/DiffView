#ifndef DIFFPENS_H
#define DIFFPENS_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 */


#include <exec/types.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>

#define DCF_CUSTOM_BACK  0x01
#define DCF_CUSTOM_TEXT  0x02

typedef struct DiffColors
{
    ULONG dc_BackCol;
    ULONG dc_TextCol;
    ULONG dc_OldCol;
    ULONG dc_NewCol;
    ULONG dc_BlankCol;
    ULONG dc_LineNoBackCol;
    ULONG dc_LineNoTextCol;
    ULONG dc_GeneralBackCol;
    ULONG dc_GeneralShineCol;
    ULONG dc_GeneralShadowCol;
    ULONG dc_OverviewOldCol;
    ULONG dc_OverviewNewCol;
    ULONG dc_WsDifferenceCol;
    ULONG dc_OverviewWsCol;
    ULONG dc_CurrentLineCol;
    UBYTE dc_Flags;
    UBYTE dc_Pad[3];
} DiffColors;

typedef struct DiffPens
{
    struct Screen *dp_Screen;
    DiffColors     dp_Colors;
    UBYTE          dp_DirectRGB;
    UBYTE          dp_Shared;
    UBYTE          dp_OwnsPens;
    UBYTE          dp_Pad;
    /* ADiffViewPens / DiffWindowRastports mapping */
    LONG           dp_PenBack;
    LONG           dp_PenText;
    LONG           dp_PenHighlight;
    LONG           dp_PenRed;
    LONG           dp_PenYellow;
    LONG           dp_PenGreen;
    LONG           dp_PenGray;
    /* Legacy aliases used by diffrender / overview */
    LONG           dp_PenOld;
    LONG           dp_PenNew;
    LONG           dp_PenBlank;
    LONG           dp_PenLineNoBack;
    LONG           dp_PenLineNoText;
    LONG           dp_PenGeneralBack;
    LONG           dp_PenGeneralShine;
    LONG           dp_PenGeneralShadow;
    LONG           dp_PenOverviewOld;
    LONG           dp_PenOverviewNew;
    LONG           dp_PenWs;
    LONG           dp_PenOverviewWs;
    LONG           dp_PenCurrent;
} DiffPens;

void DiffColors_Default(DiffColors *colors);

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

BOOL DiffPens_Init(struct DiffViewClassBase *cb, DiffPens *pens,
                   struct Screen *screen, DiffColors *colors);
void DiffPens_Free(struct DiffViewClassBase *cb, DiffPens *pens);
void DiffPens_InitLibrary(struct DiffViewClassBase *cb);
void DiffPens_ShutdownShared(struct DiffViewClassBase *cb);

void DiffPens_SetJam2(struct DiffViewClassBase *cb, struct RastPort *rp,
                      LONG apen, LONG bpen);
LONG DiffPens_LineStateBPen(DiffPens *pens, UBYTE state);

void DiffPens_SetWriteColor(struct DiffViewClassBase *cb, struct RastPort *rp,
                            DiffPens *pens, ULONG rgb, LONG pen);
void DiffPens_FillRect(struct DiffViewClassBase *cb, struct RastPort *rp,
                       DiffPens *pens, LONG x, LONG y, LONG w, LONG h,
                       ULONG rgb, LONG pen);
void DiffPens_FillPattern(struct DiffViewClassBase *cb, struct RastPort *rp,
                          DiffPens *pens, LONG x, LONG y, LONG w, LONG h,
                          ULONG rgb, LONG apen, LONG bpen);

#endif
