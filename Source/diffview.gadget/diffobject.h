#ifndef DIFFOBJECT_H
#define DIFFOBJECT_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 */


#include <exec/types.h>
#include "difffile.h"
#include "diffengine.h"
#include <utility/tagitem.h>

typedef struct DiffObject
{
    DiffFile  do_Left;
    DiffFile  do_Right;
    ULONG    *do_DiffPositions;
    ULONG     do_NumDiffPositions;
    ULONG     do_DiffPosCap;
    ULONG     do_MaxLineLen;
    STRPTR    do_OldFileName;
    STRPTR    do_NewFileName;
    ULONG     do_TabSize;
} DiffObject;

DiffFile *DiffObject_Left(DiffObject *obj);
DiffFile *DiffObject_Right(DiffObject *obj);
const STRPTR DiffObject_OldName(DiffObject *obj);
const STRPTR DiffObject_NewName(DiffObject *obj);
ULONG DiffObject_MaxLineLen(DiffObject *obj);

#endif
