/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * DiffViewTest.c - harness for diffview.gadget.
 *
 * Usage:  DiffViewTest [<oldfile> <newfile>]
 *
 * With no arguments, loads left.txt and right.txt from the current
 * directory (copied beside DiffViewTest by smakefile); falls back to a
 * built-in demo if those files are missing.
 *
 * Loads two text files, builds a DiffObject, and opens a window with
 * diffview.gadget, linecmp.gadget, and scroller.gadget objects.
 * With no arguments the built-in demo buffers are used.
 */

#define INTUI_V36_NAMES_ONLY
#define __USE_SYSBASE

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/gadgetclass.h>
#include <intuition/classes.h>
#include <graphics/gfx.h>
#include <graphics/text.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include <utility/tagitem.h>
#include <proto/utility.h>
#include <string.h>

#include <gadgets/diffview.h>
#include <gadgets/scroller.h>
#include <clib/scroller_protos.h>
#include <pragmas/scroller_pragmas.h>
#include "/test/diffview_pragmas.h"

extern struct Library *DiffViewBase;
extern struct Library *ScrollerBase;

#define GAD_DIFF     1
#define GAD_LINE     2
#define GAD_VSCROLL  3
#define GAD_HSCROLL  4

#define DV_SCROLL_W  16
#define DV_LINE_H    40
#define DV_HSCROLL_H 16
#define DV_TITLE_LEN 128

#define DV_TRACE_BUF 96

static const char demo_left_text[] =
    "alpha\n"
    "beta\n"
    "gamma line\n"
    "delta\n";

static const char demo_right_text[] =
    "alpha\n"
    "beta changed\n"
    "gamma line\n"
    "echo\n";

static void dv_Write(CONST_STRPTR text, LONG len)
{
    if (text && len > 0)
        Write(Output(), (APTR) text, len);
}

static void dv_WriteZ(CONST_STRPTR text)
{
    if (text)
        dv_Write(text, (LONG) strlen((STRPTR) text));
}

static void dv_EndLine(void)
{
    dv_Write((STRPTR) "\n", 1L);
    Flush(Output());
}

static void dv_Out(CONST_STRPTR text)
{
    dv_WriteZ(text);
    dv_EndLine();
}

static void dv_Usage(void)
{
    dv_Out((STRPTR) "Usage: DiffViewTest [<oldfile> <newfile>]");
    dv_Out((STRPTR) "  (no arguments: left.txt + right.txt, else demo)");
}

static STRPTR dv_FilePart(CONST_STRPTR path)
{
    CONST_STRPTR s;
    STRPTR last;

    last = (STRPTR) path;
    if (!path)
        return (STRPTR) "";

    for (s = path; *s != '\0'; s++)
    {
        if (*s == '/' || *s == ':' || *s == '\\')
            last = (STRPTR) (s + 1);
    }

    return last;
}

static void dv_FreeFileBuf(STRPTR buf, ULONG len)
{
    ULONG allocLen;

    if (!buf)
        return;

    allocLen = len ? len : 1;
    FreeMem(buf, allocLen);
}

static BOOL dv_LoadFile(CONST_STRPTR path, STRPTR *outBuf, ULONG *outLen)
{
    BPTR fh;
    LONG n;
    ULONG cap;
    ULONG len;
    STRPTR buf;
    STRPTR grown;

    if (!path || !outBuf || !outLen)
        return FALSE;

    *outBuf = NULL;
    *outLen = 0;

    fh = Open((STRPTR) path, MODE_OLDFILE);
    if (!fh)
        return FALSE;

    /* Read until EOF; do not rely on Seek(END) for size (some host FS
     * volumes report zero length even when the file has content). */
    cap = 256;
    len = 0;
    buf = (STRPTR) AllocMem(cap, MEMF_CLEAR);
    if (!buf)
    {
        Close(fh);
        return FALSE;
    }

    while ((n = Read(fh, buf + len, (LONG) (cap - len))) > 0)
    {
        len += (ULONG) n;
        if (len >= cap)
        {
            cap <<= 1;
            grown = (STRPTR) AllocMem(cap, MEMF_CLEAR);
            if (!grown)
            {
                FreeMem(buf, cap >> 1);
                Close(fh);
                return FALSE;
            }
            memcpy(grown, buf, len);
            FreeMem(buf, cap >> 1);
            buf = grown;
        }
    }

    Close(fh);

    if (len == 0)
    {
        buf[0] = '\0';
        *outBuf = buf;
        *outLen = 0;
        return TRUE;
    }

    *outBuf = buf;
    *outLen = len;
    return TRUE;
}

static void dv_UseDemoBuffers(STRPTR *oldBuf, ULONG *oldLen,
                            STRPTR *newBuf, ULONG *newLen,
                            UBYTE *useDemo)
{
    if (!useDemo || !oldBuf || !oldLen || !newBuf || !newLen)
        return;

    if (!*useDemo)
    {
        dv_FreeFileBuf(*oldBuf, *oldLen);
        dv_FreeFileBuf(*newBuf, *newLen);
    }

    *useDemo = 1;
    *oldBuf = (STRPTR) demo_left_text;
    *newBuf = (STRPTR) demo_right_text;
    *oldLen = (ULONG) (sizeof(demo_left_text) - 1);
    *newLen = (ULONG) (sizeof(demo_right_text) - 1);
}

static void dv_OutLabelHex(CONST_STRPTR label, APTR ptr);
static void dv_OutLabelULong(CONST_STRPTR label, ULONG value);

#define DBG(step)           dv_Out((STRPTR) "[DiffViewTest] " step)
#define DBGP(step, p)       dv_OutLabelHex((STRPTR) "[DiffViewTest] " step, (APTR) (p))
#define DBGN(step, n)       dv_OutLabelULong((STRPTR) "[DiffViewTest] " step, (ULONG) (n))

static BOOL dv_TryLoadPair(CONST_STRPTR oldPath, CONST_STRPTR newPath,
                           STRPTR *oldBuf, ULONG *oldLen,
                           STRPTR *newBuf, ULONG *newLen)
{
    if (!dv_LoadFile(oldPath, oldBuf, oldLen))
    {
        dv_Out((STRPTR) "Failed to read old file:");
        dv_Out(oldPath);
        DBGN("IoErr", (ULONG) IoErr());
        return FALSE;
    }

    if (!dv_LoadFile(newPath, newBuf, newLen))
    {
        dv_Out((STRPTR) "Failed to read new file:");
        dv_Out(newPath);
        DBGN("IoErr", (ULONG) IoErr());
        dv_FreeFileBuf(*oldBuf, *oldLen);
        *oldBuf = NULL;
        *oldLen = 0;
        return FALSE;
    }

    return TRUE;
}

static void dv_AppendHex(UBYTE *buf, ULONG *pos, ULONG value)
{
    LONG shift;
    ULONG nib;
    ULONG p;

    p = *pos;
    for (shift = 28; shift >= 0; shift -= 4)
    {
        nib = (value >> shift) & 0x0FUL;
        if (nib < 10)
            buf[p++] = (UBYTE) ('0' + nib);
        else
            buf[p++] = (UBYTE) ('A' + (nib - 10));
    }
    *pos = p;
}

static void dv_AppendULong(UBYTE *buf, ULONG *pos, ULONG value)
{
    UBYTE tmp[12];
    ULONG count;
    ULONG p;
    ULONG i;

    count = 0;
    if (value == 0)
        tmp[count++] = '0';
    else
    {
        while (value > 0)
        {
            tmp[count++] = (UBYTE) ('0' + (value % 10));
            value /= 10;
        }
    }

    p = *pos;
    for (i = count; i > 0; i--)
        buf[p++] = tmp[i - 1];
    *pos = p;
}

static void dv_OutLabelHex(CONST_STRPTR label, APTR ptr)
{
    UBYTE buf[DV_TRACE_BUF];
    ULONG pos;
    ULONG i;

    pos = 0;
    if (label)
    {
        for (i = 0; label[i] != '\0' && pos + 1 < DV_TRACE_BUF; i++)
            buf[pos++] = (UBYTE) label[i];
    }
    if (pos + 12 < DV_TRACE_BUF)
    {
        buf[pos++] = (UBYTE) ' ';
        buf[pos++] = (UBYTE) '=';
        buf[pos++] = (UBYTE) ' ';
        dv_AppendHex(buf, &pos, (ULONG) ptr);
    }
    buf[pos] = '\0';
    dv_Out((STRPTR) buf);
}

static void dv_OutLabelULong(CONST_STRPTR label, ULONG value)
{
    UBYTE buf[DV_TRACE_BUF];
    ULONG pos;
    ULONG i;

    pos = 0;
    if (label)
    {
        for (i = 0; label[i] != '\0' && pos + 1 < DV_TRACE_BUF; i++)
            buf[pos++] = (UBYTE) label[i];
    }
    if (pos + 14 < DV_TRACE_BUF)
    {
        buf[pos++] = (UBYTE) ' ';
        buf[pos++] = (UBYTE) '=';
        buf[pos++] = (UBYTE) ' ';
        dv_AppendULong(buf, &pos, value);
    }
    buf[pos] = '\0';
    dv_Out((STRPTR) buf);
}

int main(int argc, char **argv)
{
    struct Screen *screen;
    struct Window *win;
    APTR diffObj;
    Object *diffGad;
    Object *lineGad;
    Object *vScrollGad;
    Object *hScrollGad;
    Class *diffClass;
    Class *lineClass;
    Class *scrollerClass;
    ULONG err;
    STRPTR errStr;
    ULONG done;
    struct IntuiMessage *msg;
    struct TagItem createTags[10];
    struct TagItem *tags;
    ULONG gadId;
    WORD innerW;
    WORD innerH;
    WORD diffW;
    WORD diffH;
    WORD diffTop;
    WORD diffLeft;
    STRPTR oldBuf;
    STRPTR newBuf;
    ULONG oldLen;
    ULONG newLen;
    STRPTR oldPath;
    STRPTR newPath;
    STRPTR oldName;
    STRPTR newName;
    UBYTE useDemo;
    UBYTE titleBuf[DV_TITLE_LEN];
    ULONG titlePos;

    oldBuf = NULL;
    newBuf = NULL;
    oldLen = 0;
    newLen = 0;
    oldPath = NULL;
    newPath = NULL;
    useDemo = 0;

    DBG("start");

    if (argc >= 3)
    {
        oldPath = (STRPTR) argv[1];
        newPath = (STRPTR) argv[2];

        DBG("loading file pair...");
        dv_Out(oldPath);
        dv_Out(newPath);
        if (!dv_TryLoadPair(oldPath, newPath, &oldBuf, &oldLen, &newBuf, &newLen))
            return 20;

        oldName = dv_FilePart(oldPath);
        newName = dv_FilePart(newPath);
        DBGN("oldLen", oldLen);
        DBGN("newLen", newLen);
        if (oldLen == 0 && newLen == 0)
        {
            DBG("empty files, using built-in demo buffers");
            dv_UseDemoBuffers(&oldBuf, &oldLen, &newBuf, &newLen, &useDemo);
        }
    }
    else if (dv_TryLoadPair((STRPTR) "left.txt", (STRPTR) "right.txt",
                            &oldBuf, &oldLen, &newBuf, &newLen))
    {
        oldPath = (STRPTR) "left.txt";
        newPath = (STRPTR) "right.txt";
        oldName = (STRPTR) "left.txt";
        newName = (STRPTR) "right.txt";
        DBGN("oldLen", oldLen);
        DBGN("newLen", newLen);
        if (oldLen == 0 && newLen == 0)
        {
            DBG("empty left.txt/right.txt, using built-in demo buffers");
            dv_UseDemoBuffers(&oldBuf, &oldLen, &newBuf, &newLen, &useDemo);
        }
        else
            DBG("loaded left.txt + right.txt from current directory");
    }
    else
    {
        useDemo = 1;
        oldBuf = (STRPTR) demo_left_text;
        newBuf = (STRPTR) demo_right_text;
        oldLen = (ULONG) (sizeof(demo_left_text) - 1);
        newLen = (ULONG) (sizeof(demo_right_text) - 1);
        oldName = (STRPTR) "left.txt";
        newName = (STRPTR) "right.txt";
        DBG("using built-in demo buffers");
    }

    titlePos = 0;
    titleBuf[titlePos++] = (UBYTE) 'D';
    titleBuf[titlePos++] = (UBYTE) 'i';
    titleBuf[titlePos++] = (UBYTE) 'f';
    titleBuf[titlePos++] = (UBYTE) 'f';
    titleBuf[titlePos++] = (UBYTE) 'V';
    titleBuf[titlePos++] = (UBYTE) 'i';
    titleBuf[titlePos++] = (UBYTE) 'e';
    titleBuf[titlePos++] = (UBYTE) 'w';
    titleBuf[titlePos++] = (UBYTE) ':';
    titleBuf[titlePos++] = (UBYTE) ' ';

    {
        STRPTR s;
        ULONG i;

        s = oldName ? oldName : (STRPTR) "old";
        for (i = 0; s[i] != '\0' && titlePos + 16 < DV_TITLE_LEN; i++)
            titleBuf[titlePos++] = (UBYTE) s[i];
    }

    if (titlePos + 4 < DV_TITLE_LEN)
    {
        titleBuf[titlePos++] = (UBYTE) ' ';
        titleBuf[titlePos++] = (UBYTE) 'v';
        titleBuf[titlePos++] = (UBYTE) 's';
        titleBuf[titlePos++] = (UBYTE) ' ';
    }

    {
        STRPTR s;
        ULONG i;

        s = newName ? newName : (STRPTR) "new";
        for (i = 0; s[i] != '\0' && titlePos + 1 < DV_TITLE_LEN; i++)
            titleBuf[titlePos++] = (UBYTE) s[i];
    }

    titleBuf[titlePos] = '\0';

    DiffViewBase = OpenLibrary(DIFFVIEWNAME, 0);
    if (!DiffViewBase)
    {
        dv_Out((STRPTR) "Failed to open diffview.gadget");
        dv_Usage();
        if (!useDemo)
        {
            dv_FreeFileBuf(oldBuf, oldLen);
            dv_FreeFileBuf(newBuf, newLen);
        }
        return 20;
    }
    DBGP("OpenLibrary diffview", DiffViewBase);

    ScrollerBase = OpenLibrary("gadgets/scroller.gadget", 0);
    if (!ScrollerBase)
    {
        DBG("OpenLibrary scroller.gadget failed");
        CloseLibrary(DiffViewBase);
        if (!useDemo)
        {
            dv_FreeFileBuf(oldBuf, oldLen);
            dv_FreeFileBuf(newBuf, newLen);
        }
        return 20;
    }
    DBGP("OpenLibrary scroller", ScrollerBase);

    screen = LockPubScreen(NULL);
    if (!screen)
    {
        DBG("LockPubScreen failed");
        CloseLibrary(ScrollerBase);
        CloseLibrary(DiffViewBase);
        if (!useDemo)
        {
            dv_FreeFileBuf(oldBuf, oldLen);
            dv_FreeFileBuf(newBuf, newLen);
        }
        return 20;
    }
    DBGP("LockPubScreen", screen);

    err = DIFFOBJECT_ERROR_NONE;
    errStr = NULL;

    createTags[0].ti_Tag = DIFFOBJECT_OldFile;
    createTags[0].ti_Data = (ULONG) oldBuf;
    createTags[1].ti_Tag = DIFFOBJECT_OldFileLen;
    createTags[1].ti_Data = oldLen;
    createTags[2].ti_Tag = DIFFOBJECT_NewFile;
    createTags[2].ti_Data = (ULONG) newBuf;
    createTags[3].ti_Tag = DIFFOBJECT_NewFileLen;
    createTags[3].ti_Data = newLen;
    createTags[4].ti_Tag = DIFFOBJECT_OldFileName;
    createTags[4].ti_Data = (ULONG) oldName;
    createTags[5].ti_Tag = DIFFOBJECT_NewFileName;
    createTags[5].ti_Data = (ULONG) newName;
    createTags[6].ti_Tag = DIFFOBJECT_ErrorStr;
    createTags[6].ti_Data = (ULONG) &errStr;
    createTags[7].ti_Tag = DIFFOBJECT_ErrorNum;
    createTags[7].ti_Data = (ULONG) &err;
    createTags[8].ti_Tag = TAG_END;
    createTags[8].ti_Data = 0;

    DBG("CreateDiffObject...");
    diffObj = CreateDiffObject(createTags);

    if (!useDemo)
    {
        dv_FreeFileBuf(oldBuf, oldLen);
        dv_FreeFileBuf(newBuf, newLen);
        oldBuf = NULL;
        newBuf = NULL;
    }

    DBG("CreateDiffObject returned");

    if (!diffObj)
    {
        dv_Out((STRPTR) "CreateDiffObject failed");
        if (errStr)
            dv_Out(errStr);
        DBGN("DIFFOBJECT_ErrorNum", err);
        DBG("shutdown after CreateDiffObject failure");
        DBG("UnlockPubScreen...");
        UnlockPubScreen(NULL, screen);
        DBG("CloseLibrary scroller...");
        CloseLibrary(ScrollerBase);
        DBG("CloseLibrary diffview...");
        CloseLibrary(DiffViewBase);
        DBG("exit 20");
        return 20;
    }
    DBGP("CreateDiffObject", diffObj);

    DBG("OpenWindow...");
    win = OpenWindowTags(NULL,
        WA_Left, 40,
        WA_Top, 40,
        WA_Width, 640,
        WA_Height, 400,
        WA_MinWidth, 320,
        WA_MinHeight, 200,
        WA_MaxWidth, -1,
        WA_MaxHeight, -1,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE |
                  IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
                  IDCMP_INTUITICKS | IDCMP_RAWKEY | IDCMP_VANILLAKEY |
                  IDCMP_IDCMPUPDATE,
        WA_CloseGadget, TRUE,
        WA_DragBar, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_PubScreen, (ULONG) screen,
        WA_Title, (ULONG) titleBuf,
        TAG_END);

    if (!win)
    {
        DBG("OpenWindow failed");
        FreeDiffObject(diffObj);
        UnlockPubScreen(0, screen);
        CloseLibrary(ScrollerBase);
        CloseLibrary(DiffViewBase);
        return 20;
    }
    DBGP("OpenWindow", win);

    innerW = (WORD) (win->Width - win->BorderLeft - win->BorderRight);
    innerH = (WORD) (win->Height - win->BorderTop - win->BorderBottom);
    diffLeft = 8;
    diffTop = 24;
    diffW = (WORD) (innerW - diffLeft * 2 - DV_SCROLL_W);
    diffH = (WORD) (innerH - diffTop - DV_LINE_H - DV_HSCROLL_H - 8);
    if (diffW < 80)
        diffW = 80;
    if (diffH < 60)
        diffH = 60;

    scrollerClass = SCROLLER_GetClass();
    DBGP("SCROLLER_GetClass", scrollerClass);
    if (!scrollerClass)
    {
        DBG("SCROLLER_GetClass returned NULL");
        CloseWindow(win);
        FreeDiffObject(diffObj);
        UnlockPubScreen(0, screen);
        CloseLibrary(ScrollerBase);
        CloseLibrary(DiffViewBase);
        return 20;
    }

    DBG("LINECMP_GetClass...");
    lineClass = LINECMP_GetClass();
    DBGP("LINECMP_GetClass", lineClass);
    if (!lineClass)
    {
        DBG("LINECMP_GetClass returned NULL");
        CloseWindow(win);
        FreeDiffObject(diffObj);
        UnlockPubScreen(0, screen);
        CloseLibrary(ScrollerBase);
        CloseLibrary(DiffViewBase);
        return 20;
    }

    DBG("NewObject linecmp...");
    lineGad = NewObject(lineClass, NULL,
        GA_ID, GAD_LINE,
        GA_Left, diffLeft,
        GA_Top, (WORD) (diffTop + diffH + DV_HSCROLL_H + 4),
        GA_Width, diffW,
        GA_Height, DV_LINE_H,
        GA_RelVerify, TRUE,
        LINECMP_Screen, (ULONG) screen,
        TAG_END);
    DBGP("NewObject linecmp", lineGad);

    DBG("DIFFVIEW_GetClass...");
    diffClass = DIFFVIEW_GetClass();
    DBGP("DIFFVIEW_GetClass", diffClass);
    if (!diffClass)
    {
        DBG("DIFFVIEW_GetClass returned NULL");
        if (lineGad)
            DisposeObject(lineGad);
        CloseWindow(win);
        FreeDiffObject(diffObj);
        UnlockPubScreen(0, screen);
        CloseLibrary(ScrollerBase);
        CloseLibrary(DiffViewBase);
        return 20;
    }

    DBG("NewObject vscroll...");
    vScrollGad = NewObject(scrollerClass, NULL,
        GA_ID, GAD_VSCROLL,
        GA_Left, (WORD) (diffLeft + diffW + 2),
        GA_Top, diffTop,
        GA_Width, DV_SCROLL_W,
        GA_Height, diffH,
        GA_RelVerify, TRUE,
        SCROLLER_Orientation, SCROLLER_VERTICAL,
        SCROLLER_Arrows, TRUE,
        TAG_END);
    DBGP("NewObject vscroll", vScrollGad);

    DBG("NewObject hscroll...");
    hScrollGad = NewObject(scrollerClass, NULL,
        GA_ID, GAD_HSCROLL,
        GA_Left, diffLeft,
        GA_Top, (WORD) (diffTop + diffH + 2),
        GA_Width, diffW,
        GA_Height, DV_HSCROLL_H,
        GA_RelVerify, TRUE,
        SCROLLER_Orientation, SCROLLER_HORIZONTAL,
        SCROLLER_Arrows, TRUE,
        TAG_END);
    DBGP("NewObject hscroll", hScrollGad);

    DBG("NewObject diffview...");
    diffGad = NewObject(diffClass, NULL,
        GA_ID, GAD_DIFF,
        GA_Left, diffLeft,
        GA_Top, diffTop,
        GA_Width, diffW,
        GA_Height, diffH,
        GA_RelVerify, TRUE,
        DIFFVIEW_Screen, (ULONG) screen,
        DIFFVIEW_DiffObject, (ULONG) diffObj,
        DIFFVIEW_LineCmp, (ULONG) lineGad,
        DIFFVIEW_VertScroller, (ULONG) vScrollGad,
        DIFFVIEW_HorizScroller, (ULONG) hScrollGad,
        DIFFVIEW_CurrentLine, 1,
        TAG_END);
    DBGP("NewObject diffview", diffGad);
    DBGN("diffview Width", (ULONG) ((struct Gadget *) diffGad)->Width);
    DBGN("diffview Height", (ULONG) ((struct Gadget *) diffGad)->Height);

    if (!diffGad || !lineGad || !vScrollGad || !hScrollGad)
    {
        DBG("gadget creation failed");
        if (diffGad)
            DisposeObject(diffGad);
        if (lineGad)
            DisposeObject(lineGad);
        if (vScrollGad)
            DisposeObject(vScrollGad);
        if (hScrollGad)
            DisposeObject(hScrollGad);
        CloseWindow(win);
        FreeDiffObject(diffObj);
        UnlockPubScreen(0, screen);
        CloseLibrary(ScrollerBase);
        CloseLibrary(DiffViewBase);
        return 20;
    }

    DBG("AddGadget diffview...");
    AddGadget(win, (struct Gadget *) diffGad, -1);
    DBG("AddGadget vscroll...");
    AddGadget(win, (struct Gadget *) vScrollGad, -1);
    DBG("AddGadget hscroll...");
    AddGadget(win, (struct Gadget *) hScrollGad, -1);
    DBG("AddGadget linecmp...");
    AddGadget(win, (struct Gadget *) lineGad, -1);

    DBG("ActivateGadget diffview...");
    ActivateGadget((struct Gadget *) diffGad, win, NULL);

    DBG("RefreshWindowFrame...");
    RefreshWindowFrame(win);
    DBG("RefreshGList...");
    RefreshGList((struct Gadget *) diffGad, win, NULL, -1);
    DBG("entering event loop");

    done = 0;
    while (!done)
    {
        Wait((ULONG) (1 << win->UserPort->mp_SigBit));
        while ((msg = (struct IntuiMessage *) GetMsg(win->UserPort)))
        {
            if (msg->Class == IDCMP_CLOSEWINDOW)
            {
                done = 1;
            }
            else if (msg->Class == IDCMP_IDCMPUPDATE)
            {
                tags = (struct TagItem *) msg->IAddress;
                gadId = GetTagData(GA_ID, 0, tags);
                if (gadId == GAD_VSCROLL)
                {
                    SetGadgetAttrs((struct Gadget *) diffGad, win, NULL,
                                   DIFFVIEW_ViewOffsetTop,
                                   (ULONG) GetTagData(SCROLLER_Top, 0, tags),
                                   TAG_END);
                }
                else if (gadId == GAD_HSCROLL)
                {
                    SetGadgetAttrs((struct Gadget *) diffGad, win, NULL,
                                   DIFFVIEW_ViewOffsetLeft,
                                   (ULONG) GetTagData(SCROLLER_Top, 0, tags),
                                   TAG_END);
                }
            }
            else if (msg->Class == IDCMP_NEWSIZE)
            {
                innerW = (WORD) (win->Width - win->BorderLeft - win->BorderRight);
                innerH = (WORD) (win->Height - win->BorderTop - win->BorderBottom);
                diffW = (WORD) (innerW - diffLeft * 2 - DV_SCROLL_W);
                diffH = (WORD) (innerH - diffTop - DV_LINE_H - DV_HSCROLL_H - 8);
                if (diffW < 80)
                    diffW = 80;
                if (diffH < 60)
                    diffH = 60;

                SetGadgetAttrs((struct Gadget *) diffGad, win, NULL,
                               GA_Width, (LONG) diffW,
                               GA_Height, (LONG) diffH,
                               TAG_END);
                SetGadgetAttrs((struct Gadget *) vScrollGad, win, NULL,
                               GA_Left, (LONG) (diffLeft + diffW + 2),
                               GA_Height, (LONG) diffH,
                               TAG_END);
                SetGadgetAttrs((struct Gadget *) hScrollGad, win, NULL,
                               GA_Top, (LONG) (diffTop + diffH + 2),
                               GA_Width, (LONG) diffW,
                               TAG_END);
                SetGadgetAttrs((struct Gadget *) lineGad, win, NULL,
                               GA_Top, (LONG) (diffTop + diffH + DV_HSCROLL_H + 4),
                               GA_Width, (LONG) diffW,
                               TAG_END);

                RefreshWindowFrame(win);
                RefreshGList((struct Gadget *) diffGad, win, NULL, -1);
            }
            ReplyMsg((struct Message *) msg);
        }
    }

    DBG("shutdown: clear diffview references");
    SetGadgetAttrs((struct Gadget *) diffGad, win, NULL,
                   DIFFVIEW_DiffObject, 0,
                   DIFFVIEW_LineCmp, 0,
                   DIFFVIEW_VertScroller, 0,
                   DIFFVIEW_HorizScroller, 0,
                   TAG_END);

    DBG("shutdown: RemoveGadget diffview");
    RemoveGadget(win, (struct Gadget *) diffGad);
    DBG("shutdown: RemoveGadget vscroll");
    RemoveGadget(win, (struct Gadget *) vScrollGad);
    DBG("shutdown: RemoveGadget hscroll");
    RemoveGadget(win, (struct Gadget *) hScrollGad);
    DBG("shutdown: RemoveGadget linecmp");
    RemoveGadget(win, (struct Gadget *) lineGad);
    DBG("shutdown: DisposeObject diffview");
    DisposeObject(diffGad);
    DBG("shutdown: DisposeObject vscroll");
    DisposeObject(vScrollGad);
    DBG("shutdown: DisposeObject hscroll");
    DisposeObject(hScrollGad);
    DBG("shutdown: DisposeObject linecmp");
    DisposeObject(lineGad);
    DBG("shutdown: CloseWindow");
    CloseWindow(win);
    DBG("shutdown: FreeDiffObject");
    FreeDiffObject(diffObj);
    DBG("shutdown: UnlockPubScreen");
    UnlockPubScreen(NULL, screen);
    DBG("shutdown: CloseLibrary scroller");
    CloseLibrary(ScrollerBase);
    DBG("shutdown: CloseLibrary diffview");
    CloseLibrary(DiffViewBase);
    DBG("done");
    return 0;
}
