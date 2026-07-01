#ifndef DIFFENGINE_H
#define DIFFENGINE_H
/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2005-2012 Matthias Hertel
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView/src/diffengine/DiffEngine.cpp by Uwe Rosner
 * in turn derived from https://github.com/mathertel/Diff
 */


#include <exec/types.h>
#include "difffile.h"

#include "diffviewclass.h"

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

typedef struct DiffEngine
{
    DiffFile *de_LeftIn;
    DiffFile *de_RightIn;
    DiffFile *de_LeftOut;
    DiffFile *de_RightOut;
    ULONG    *de_DiffPositions;
    ULONG     de_NumDiffPositions;
    ULONG     de_DiffCap;
    LONG     *de_DownVector;
    LONG     *de_UpVector;
    ULONG     de_Max;
    ULONG     de_NumInserted;
    ULONG     de_NumDeleted;
    ULONG     de_NumChanged;
    ULONG     de_Percent;
    ULONG     de_PercentIncrement;
    ULONG     de_NotifyIncrement;
    ULONG     de_NextNotifyPosition;
    ULONG     de_LastReportedPercent;
    DIFFOBJPROGCB de_ProgressCB;
    APTR      de_ProgressData;
    UBYTE     de_Cancelled;
} DiffEngine;

void DiffEngine_Init(DiffEngine *eng);
void DiffEngine_Free(struct DiffViewClassBase *cb, DiffEngine *eng);
BOOL DiffEngine_Compare(struct DiffViewClassBase *cb, DiffEngine *eng,
                        DiffFile *leftIn, DiffFile *rightIn,
                        DiffFile *leftOut, DiffFile *rightOut,
                        DIFFOBJPROGCB progCB, APTR progData);

#endif
