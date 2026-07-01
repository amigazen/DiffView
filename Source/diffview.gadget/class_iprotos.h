#ifndef DIFFVIEW_CLASS_IPROTOS_H
#define DIFFVIEW_CLASS_IPROTOS_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 */


struct DiffViewClassBase *ASM LibInit (
    REG (d0) struct DiffViewClassBase *cb,
    REG (a0) BPTR seglist,
    REG (a6) struct Library *sysbase);
struct DiffViewClassBase *ASM LibOpen (
    REG (a6) struct DiffViewClassBase *cb);
BPTR ASM LibClose (REG (a6) struct DiffViewClassBase *cb);
BPTR ASM LibExpunge (REG (a6) struct DiffViewClassBase *cb);

Class *ASM GetEngine (REG (a6) struct DiffViewClassBase *cb);
Class *ASM DIFFVIEW_GetClass (REG (a6) struct DiffViewClassBase *cb);
Class *ASM LINECMP_GetClass (REG (a6) struct DiffViewClassBase *cb);
APTR ASM L_CreateDiffObject (REG (a6) struct DiffViewClassBase *cb,
                             REG (a0) struct TagItem *tags);
void ASM L_FreeDiffObject (REG (a6) struct DiffViewClassBase *cb,
                           REG (a0) APTR obj);
BOOL ASM L_GetDiffObjectAttr (REG (a6) struct DiffViewClassBase *cb,
                              REG (a0) APTR obj,
                              REG (d0) ULONG attr,
                              REG (a1) APTR storage);

Class *DiffViewCreateClass (struct DiffViewClassBase *cb);
void   DiffViewDeleteClass (struct DiffViewClassBase *cb, Class *cl);
Class *LineCmpCreateClass (struct DiffViewClassBase *cb);
void   LineCmpDeleteClass (struct DiffViewClassBase *cb, Class *cl);

APTR CreateDiffObject (struct DiffViewClassBase *cb, struct TagItem *tags);
APTR CreateDiffObjectTags (Tag tag1, ...);
void FreeDiffObject (struct DiffViewClassBase *cb, APTR DiffObj);
BOOL GetDiffObjectAttr (struct DiffViewClassBase *cb, APTR DiffObj,
                        ULONG Attr, APTR Storage);

#endif
