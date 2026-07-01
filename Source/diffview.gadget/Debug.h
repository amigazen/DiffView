#ifndef DEBUG_H
#define DEBUG_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 */


#ifdef DIFFVIEW_DEBUG

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

void dv_TraceMsg(struct DiffViewClassBase *cb, CONST_STRPTR msg);
void dv_TraceHex(struct DiffViewClassBase *cb, CONST_STRPTR label, APTR ptr);
void dv_TraceULong(struct DiffViewClassBase *cb, CONST_STRPTR label, ULONG value);

#define DV_T(cb, msg)         dv_TraceMsg((cb), (CONST_STRPTR) (msg))
#define DV_TH(cb, label, ptr) dv_TraceHex((cb), (CONST_STRPTR) (label), (ptr))
#define DV_TU(cb, label, val) dv_TraceULong((cb), (CONST_STRPTR) (label), (ULONG) (val))

#else

#define DV_T(cb, msg)
#define DV_TH(cb, label, ptr)
#define DV_TU(cb, label, val)

#endif

#endif
