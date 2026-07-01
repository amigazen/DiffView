#ifndef DIFFSELECT_H
#define DIFFSELECT_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 */


/*
 * diffselect.h - mouse text selection for diffview.gadget.
 *
 * Ported from ADiffView/src/textselect/DynamicSelection.cpp and
 * ADiffView/src/diffengine/SelectableDiffFile.cpp.
 */

#include <exec/types.h>
#include "difffile.h"

typedef struct DiffSelect
{
    LONG ds_StartLineId;
    LONG ds_StartColumnId;
    LONG ds_MinLineId;
    LONG ds_MinLineColumnId;
    LONG ds_MaxLineId;
    LONG ds_MaxLineColumnId;
} DiffSelect;

void DiffSelect_Init(DiffSelect *sel);
void DiffSelect_Clear(DiffSelect *sel);
void DiffSelect_Start(DiffSelect *sel, DiffFile *file,
                      ULONG lineId, ULONG columnId);
void DiffSelect_Update(DiffSelect *sel, DiffFile *file,
                       ULONG lineId, ULONG columnId);

BOOL DiffSelect_IsSelected(const DiffSelect *sel, DiffFile *file,
                           ULONG lineId, ULONG columnId);
LONG DiffSelect_GetNumMarkedChars(const DiffSelect *sel, DiffFile *file,
                                  ULONG lineId, ULONG columnId);
LONG DiffSelect_GetNextSelectionStart(const DiffSelect *sel, DiffFile *file,
                                      ULONG lineId, ULONG columnId);
LONG DiffSelect_GetNumNormalChars(DiffSelect *sel, DiffFile *file,
                                  ULONG lineId, ULONG columnId);
ULONG DiffSelect_GetTotalChars(const DiffSelect *sel, DiffFile *file);

#endif
