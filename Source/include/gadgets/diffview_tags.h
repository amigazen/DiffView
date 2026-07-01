#ifndef GADGETS_DIFFVIEW_TAGS_H
#define GADGETS_DIFFVIEW_TAGS_H
/*
**	$VER: diffview_tags.h 1.0 (30.6.2026)
**
**	SPDX-License-Identifier: Apache-2.0
**	SPDX-FileCopyrightText: 2026 amigazen project
**
**	Shared tag and type definitions for diffview.gadget.
**	Included by the public client header and by the private class header.
**
*/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

#ifndef INTUITION_GADGETCLASS_H
#include <intuition/gadgetclass.h>
#endif

#ifndef INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif

#define DIFFVIEWCLASS       "diffview.gadget"
#define LINECMPCLASS        "linecmp.gadget"

#define DIFFVIEW_TAGBASE    (TAG_USER + 0xFC0000UL)
#define LINECMP_TAGBASE     (TAG_USER + 0xFC1000UL)
#define DIFFOBJECT_TAGBASE  (TAG_USER + 0xFC2000UL)

#define DIFFVIEW_CurrentLine        (DIFFVIEW_TAGBASE + 0x01)
#define DIFFVIEW_DiffObject         (DIFFVIEW_TAGBASE + 0x02)
#define DIFFVIEW_Font               (DIFFVIEW_TAGBASE + 0x03)
#define DIFFVIEW_HorizScroller      (DIFFVIEW_TAGBASE + 0x04)
#define DIFFVIEW_LineCmp            (DIFFVIEW_TAGBASE + 0x05)
#define DIFFVIEW_Screen             (DIFFVIEW_TAGBASE + 0x06)
#define DIFFVIEW_TabSize            (DIFFVIEW_TAGBASE + 0x07)
#define DIFFVIEW_VertScroller       (DIFFVIEW_TAGBASE + 0x08)
#define DIFFVIEW_ViewOffsetTop      (DIFFVIEW_TAGBASE + 0x09)
#define DIFFVIEW_ViewOffsetLeft     (DIFFVIEW_TAGBASE + 0x0A)

#define DIFFVIEW_BackCol            (DIFFVIEW_TAGBASE + 0x10)
#define DIFFVIEW_TextCol            (DIFFVIEW_TAGBASE + 0x11)
#define DIFFVIEW_OldCol             (DIFFVIEW_TAGBASE + 0x12)
#define DIFFVIEW_NewCol             (DIFFVIEW_TAGBASE + 0x13)
#define DIFFVIEW_BlankCol           (DIFFVIEW_TAGBASE + 0x14)
#define DIFFVIEW_LineNoBackCol      (DIFFVIEW_TAGBASE + 0x15)
#define DIFFVIEW_LineNoTextCol      (DIFFVIEW_TAGBASE + 0x16)
#define DIFFVIEW_GeneralBackCol     (DIFFVIEW_TAGBASE + 0x17)
#define DIFFVIEW_GeneralShineCol    (DIFFVIEW_TAGBASE + 0x18)
#define DIFFVIEW_GeneralShadowCol   (DIFFVIEW_TAGBASE + 0x19)
#define DIFFVIEW_OverviewOldCol     (DIFFVIEW_TAGBASE + 0x1A)
#define DIFFVIEW_OverviewNewCol     (DIFFVIEW_TAGBASE + 0x1B)
#define DIFFVIEW_WsDifferenceCol    (DIFFVIEW_TAGBASE + 0x1C)
#define DIFFVIEW_OverviewWsCol      (DIFFVIEW_TAGBASE + 0x1D)

#define LINECMP_Font                (LINECMP_TAGBASE + 0x01)
#define LINECMP_HorizScroller       (LINECMP_TAGBASE + 0x02)
#define LINECMP_Screen              (LINECMP_TAGBASE + 0x03)
#define LINECMP_TabSize             (LINECMP_TAGBASE + 0x04)
#define LINECMP_ViewOffsetLeft      (LINECMP_TAGBASE + 0x05)
#define LINECMP_OldLine             (LINECMP_TAGBASE + 0x06)
#define LINECMP_NewLine             (LINECMP_TAGBASE + 0x07)

#define DIFFOBJECT_OldFile          (DIFFOBJECT_TAGBASE + 0x01)
#define DIFFOBJECT_OldFileLen       (DIFFOBJECT_TAGBASE + 0x02)
#define DIFFOBJECT_NewFile          (DIFFOBJECT_TAGBASE + 0x03)
#define DIFFOBJECT_NewFileLen       (DIFFOBJECT_TAGBASE + 0x04)
#define DIFFOBJECT_Diffs            (DIFFOBJECT_TAGBASE + 0x05)
#define DIFFOBJECT_DiffsLen         (DIFFOBJECT_TAGBASE + 0x06)
#define DIFFOBJECT_ErrorStr         (DIFFOBJECT_TAGBASE + 0x07)
#define DIFFOBJECT_ErrorNum         (DIFFOBJECT_TAGBASE + 0x08)
#define DIFFOBJECT_OldFileName      (DIFFOBJECT_TAGBASE + 0x09)
#define DIFFOBJECT_NewFileName      (DIFFOBJECT_TAGBASE + 0x0A)
#define DIFFOBJECT_WsUnimportant    (DIFFOBJECT_TAGBASE + 0x0B)
#define DIFFOBJECT_ProgressCB       (DIFFOBJECT_TAGBASE + 0x0C)
#define DIFFOBJECT_ProgCBData       (DIFFOBJECT_TAGBASE + 0x0D)

#define DIFFOBJECT_DiffPositions    (DIFFOBJECT_TAGBASE + 0x20)

#define DIFFOBJECT_ERROR_NONE           0
#define DIFFOBJECT_ERROR_NOMEM          1
#define DIFFOBJECT_ERROR_NOINPUT        2
#define DIFFOBJECT_ERROR_DIFFPARSE      3
#define DIFFOBJECT_ERROR_CANCELLED      4

typedef void (*DIFFOBJPROGCB)(ULONG percent, APTR userdata);

struct DiffPosition
{
    ULONG *dp_Positions;
    ULONG  dp_Count;
};

#define DIFFVIEWNAME    "diffview.gadget"

#endif /* GADGETS_DIFFVIEW_TAGS_H */
