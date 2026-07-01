#ifndef DIFFCLIP_H
#define DIFFCLIP_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based partially on ADiffView by Uwe Rosner.
 */


#include <exec/types.h>
#include "diffselect.h"
#include "difffile.h"

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

BOOL DiffClip_CopySelection(struct DiffViewClassBase *cb,
                            DiffSelect *sel, DiffFile *file);

#endif
