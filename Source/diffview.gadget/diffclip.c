/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based partially on ADiffView by Uwe Rosner.
 *
 * diffclip.c - copy text selection to the Amiga clipboard.
 *
 * Ported from ADiffView CmdCopySelection.cpp and Clipboard.cpp.
 */

#include "classbase.h"
#include "diffclip.h"
#include "diffline.h"

#include <devices/clipboard.h>

static BOOL dc_WriteLong(struct IOClipReq *ioclr, LONG value)
{
    ioclr->io_Data = (APTR) &value;
    ioclr->io_Length = 4;
    ioclr->io_Command = CMD_WRITE;
    DoIO((struct IORequest *) ioclr);
    if (ioclr->io_Error)
        return FALSE;
    if (ioclr->io_Actual != 4)
        return FALSE;
    return TRUE;
}

static BOOL dc_WriteChunk(struct IOClipReq *ioclr, CONST_STRPTR text, ULONG len)
{
    if (len == 0)
        return TRUE;

    ioclr->io_Data = (APTR) text;
    ioclr->io_Length = len;
    ioclr->io_Command = CMD_WRITE;
    DoIO((struct IORequest *) ioclr);
    if (ioclr->io_Error)
        return FALSE;
    if (ioclr->io_Actual != (LONG) len)
        return FALSE;
    return TRUE;
}

BOOL DiffClip_CopySelection(struct DiffViewClassBase *cb,
                            DiffSelect *sel, DiffFile *file)
{
    struct MsgPort *port;
    struct IOClipReq *ioclr;
    ULONG totalChars;
    ULONG numLines;
    ULONG fromLine;
    ULONG toLine;
    ULONG i;
    ULONG textLen;
    ULONG oddPad;
    ULONG formLen;
    LONG formTag;
    LONG ftxtTag;
    LONG chrsTag;
    BYTE devErr;
    DiffLine *line;
    CONST_STRPTR text;
    LONG startCol;
    LONG numChars;
    BOOL appendNl;
    BYTE updateOk;

    if (!cb || !sel || !file)
        return FALSE;

    totalChars = DiffSelect_GetTotalChars(sel, file);
    if (totalChars == 0)
        return FALSE;

    fromLine = (ULONG) sel->ds_MinLineId;
    toLine = (ULONG) sel->ds_MaxLineId;
    numLines = toLine - fromLine + 1;
    totalChars += (numLines - 1);

    port = CreateMsgPort();
    if (!port)
        return FALSE;

    ioclr = (struct IOClipReq *) CreateIORequest(port, sizeof(struct IOClipReq));
    if (!ioclr)
    {
        DeleteMsgPort(port);
        return FALSE;
    }

    devErr = OpenDevice("clipboard.device", 0,
                        (struct IORequest *) ioclr, 0);
    if (devErr)
    {
        DeleteIORequest(ioclr);
        DeleteMsgPort(port);
        return FALSE;
    }

    oddPad = totalChars & 1;
    formLen = totalChars + 12;
    if (oddPad)
        formLen++;

    ioclr->io_Offset = 0;
    ioclr->io_Error = 0;
    ioclr->io_ClipID = 0;
    ioclr->io_Command = CMD_WRITE;

    formTag = (LONG) 0x464F524D; /* FORM */
    ftxtTag = (LONG) 0x46545854; /* FTXT */
    chrsTag = (LONG) 0x43485253; /* CHRS */

    if (!dc_WriteLong(ioclr, formTag) ||
        !dc_WriteLong(ioclr, (LONG) formLen) ||
        !dc_WriteLong(ioclr, ftxtTag) ||
        !dc_WriteLong(ioclr, chrsTag) ||
        !dc_WriteLong(ioclr, (LONG) totalChars))
    {
        CloseDevice((struct IORequest *) ioclr);
        DeleteIORequest(ioclr);
        DeleteMsgPort(port);
        return FALSE;
    }

    for (i = fromLine; i <= toLine; i++)
    {
        line = DiffFile_Line(file, i);
        text = line ? DiffLine_Text(line) : (CONST_STRPTR) "";
        textLen = line ? DiffLine_TextLen(line) : 0;
        startCol = 0;
        numChars = 0;
        appendNl = TRUE;

        if (i == fromLine)
        {
            startCol = DiffSelect_GetNextSelectionStart(sel, file, i, 0);
            if (startCol < 0)
                startCol = 0;
            if ((ULONG) startCol > textLen)
                startCol = (LONG) textLen;
            numChars = (LONG) textLen - startCol;
            text = text + startCol;
        }
        else if (i == toLine)
        {
            numChars = DiffSelect_GetNumMarkedChars(sel, file, i, 0);
            if (numChars < 0)
                numChars = 0;
            if ((ULONG) numChars > textLen)
                numChars = (LONG) textLen;
            appendNl = FALSE;
        }
        else
        {
            numChars = (LONG) textLen;
        }

        if (!dc_WriteChunk(ioclr, text, (ULONG) numChars))
        {
            CloseDevice((struct IORequest *) ioclr);
            DeleteIORequest(ioclr);
            DeleteMsgPort(port);
            return FALSE;
        }

        if (appendNl && i < toLine)
        {
            if (!dc_WriteChunk(ioclr, (CONST_STRPTR) "\n", 1))
            {
                CloseDevice((struct IORequest *) ioclr);
                DeleteIORequest(ioclr);
                DeleteMsgPort(port);
                return FALSE;
            }
        }
    }

    if (oddPad)
    {
        if (!dc_WriteChunk(ioclr, (CONST_STRPTR) "", 1))
        {
            CloseDevice((struct IORequest *) ioclr);
            DeleteIORequest(ioclr);
            DeleteMsgPort(port);
            return FALSE;
        }
    }

    ioclr->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *) ioclr);

    updateOk = (ioclr->io_Error == 0) ? 1 : 0;

    CloseDevice((struct IORequest *) ioclr);
    DeleteIORequest(ioclr);
    DeleteMsgPort(port);

    return updateOk ? TRUE : FALSE;
}
