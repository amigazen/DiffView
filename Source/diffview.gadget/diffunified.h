#ifndef DIFFUNIFIED_H
#define DIFFUNIFIED_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 */


#include <exec/types.h>
#include <gadgets/diffview_tags.h>
#include "diffobject.h"

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

BOOL DiffUnified_Build(struct DiffViewClassBase *cb, DiffObject *obj,
                      STRPTR oldBuf, ULONG oldLen,
                      STRPTR newBuf, ULONG newLen,
                      STRPTR diffBuf, ULONG diffLen,
                      UBYTE wsUnimportant,
                      DIFFOBJPROGCB progCB, APTR progData);

#endif
