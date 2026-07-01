#ifndef GADGETS_DIFFVIEW_H
#define GADGETS_DIFFVIEW_H
/*
**	$VER: diffview.h 1.0 (30.6.2026)
**
**	SPDX-License-Identifier: Apache-2.0
**	SPDX-FileCopyrightText: 2026 amigazen project
**
**	Public client interface for diffview.gadget.
**
**	Applications and test harnesses include this header.  The class
**	library itself uses diffviewclass.h instead (private header).
**
*/

#include <gadgets/diffview_tags.h>

#ifdef __cplusplus
extern "C" {
#endif

Class *DIFFVIEW_GetClass(void);
Class *LINECMP_GetClass(void);

APTR CreateDiffObject(struct TagItem *tags);
APTR CreateDiffObjectTags(Tag tag1, ...);
void FreeDiffObject(APTR DiffObj);
BOOL GetDiffObjectAttr(APTR DiffObj, ULONG Attr, APTR Storage);

#ifdef __cplusplus
}
#endif

#endif /* GADGETS_DIFFVIEW_H */
