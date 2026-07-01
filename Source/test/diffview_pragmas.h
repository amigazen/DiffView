/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * diffview_pragmas.h - #pragma libcall offsets for diffview.gadget
 *
 * Offsets MUST match FuncTab[] in classbase.c (first user vector at -30):
 *   1e  GetEngine (_GetEngine)
 *   24  DIFFVIEW_GetClass
 *   2a  LINECMP_GetClass
 *   30  CreateDiffObject
 *   36  FreeDiffObject
 *   3c  GetDiffObjectAttr
 */

#ifndef DIFFVIEW_PRAGMAS_H
#define DIFFVIEW_PRAGMAS_H

extern struct Library *DiffViewBase;

#pragma libcall DiffViewBase DIFFVIEW_GetClass     24 00
#pragma libcall DiffViewBase LINECMP_GetClass      2a 00
#pragma libcall DiffViewBase CreateDiffObject      30 801
#pragma libcall DiffViewBase FreeDiffObject        36 801
#pragma libcall DiffViewBase GetDiffObjectAttr     3c 09803

#endif
