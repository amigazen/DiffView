/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * diffview_stubs.c - test harness glue for diffview.gadget LVOs.
 *
 * Library vectors are reached through SAS/C #pragma libcall entries
 * in diffview_pragmas.h (DiffViewBase must be set before any call).
 */

#include <exec/types.h>

#include <gadgets/diffview.h>
#include "/test/diffview_pragmas.h"

struct Library *DiffViewBase = NULL;
