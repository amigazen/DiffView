#ifndef DIFFLINE_H
#define DIFFLINE_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView by Uwe Rosner.
 */


#include <exec/types.h>

/*
 * Per-line diff state values.  Ported from ADiffView DiffLine::LineState.
 */
#define DLS_NORMAL      0
#define DLS_CHANGED     1
#define DLS_ADDED       2
#define DLS_DELETED     3
#define DLS_UNDEFINED   4
#define DLS_WS_ONLY     5   /* differs only in whitespace when WsUnimportant */

typedef struct TextPositionInfo
{
    ULONG tpi_RemainingSpaces;
    ULONG tpi_RemainingChars;
    ULONG tpi_SrcTextColumn;
} TextPositionInfo;

typedef struct DiffLine
{
    STRPTR  dl_Text;
    ULONG   dl_TextLen;
    UBYTE   dl_State;
    STRPTR  dl_LineNumText;
    ULONG   dl_Token;
    UBYTE   dl_Linked;      /* text owned elsewhere when TRUE */
} DiffLine;

#ifndef DIFFVIEW_CLASSBASE_H
struct DiffViewClassBase;
#endif

void DiffLine_Init(DiffLine *line, STRPTR text, UBYTE skipLeading);
void DiffLine_InitLinked(DiffLine *line, STRPTR text, UBYTE state,
                         STRPTR lineNumText);
void DiffLine_Free(struct DiffViewClassBase *cb, DiffLine *line);
const STRPTR DiffLine_Text(const DiffLine *line);
ULONG DiffLine_TextLen(const DiffLine *line);
UBYTE DiffLine_State(const DiffLine *line);
void DiffLine_SetState(DiffLine *line, UBYTE state);
const STRPTR DiffLine_LineNumText(const DiffLine *line);
void DiffLine_SetLineNumText(DiffLine *line, STRPTR text);
ULONG DiffLine_Token(const DiffLine *line);
ULONG DiffLine_RenderColumn(const DiffLine *line, ULONG docCol, ULONG tabSize);
ULONG DiffLine_DocumentColumn(const DiffLine *line, ULONG renderCol, ULONG tabSize);
void DiffLine_GetTextPositionInfo(const DiffLine *line, TextPositionInfo *info,
                                  ULONG resultingCol, ULONG tabSize);

#endif
