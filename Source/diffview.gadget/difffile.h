#ifndef DIFFFILE_H
#define DIFFFILE_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 */


#include <exec/types.h>
#include "diffline.h"

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

typedef struct DiffFile
{
    DiffLine **df_Lines;
    ULONG      df_NumLines;
    ULONG      df_Capacity;
} DiffFile;

void DiffFile_Init(DiffFile *file);
void DiffFile_Free(struct DiffViewClassBase *cb, DiffFile *file);
BOOL DiffFile_AddLine(struct DiffViewClassBase *cb, DiffFile *file,
                      DiffLine *line);
BOOL DiffFile_AddEmptyLine(struct DiffViewClassBase *cb, DiffFile *file);
DiffLine *DiffFile_Line(DiffFile *file, ULONG index);
ULONG DiffFile_NumLines(const DiffFile *file);
ULONG DiffFile_NumDigits(ULONG number);
BOOL DiffFile_CollectLineNumbers(struct DiffViewClassBase *cb, DiffFile *file,
                                 ULONG maxLines);
ULONG DiffFile_MaxRenderWidth(const DiffFile *file, ULONG tabSize);

#endif
