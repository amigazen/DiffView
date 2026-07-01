/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * classbase.c - library lifecycle and LVO entry points for diffview.gadget.
 *
 * ROM tag and vector table live in classinit.asm.
 * FuncTab LVO hex offsets (SAS/C #pragma libcall):
 *   1e GetEngine  24 DIFFVIEW_GetClass  2a LINECMP_GetClass
 *   30 CreateDiffObject  36 FreeDiffObject  3c GetDiffObjectAttr
 */

#include "classbase.h"
#include "Debug.h"
#include "diffpens.h"

extern struct DiffViewClassBase *do_LibBase;

#define WANTED_LIBVER 39

static void dvb_CloseBases (struct DiffViewClassBase *cb)
{
    if (UtilityBase)
    {
        CloseLibrary (UtilityBase);
        UtilityBase = NULL;
    }
    if (GfxBase)
    {
        CloseLibrary (GfxBase);
        GfxBase = NULL;
    }
    if (IntuitionBase)
    {
        CloseLibrary (IntuitionBase);
        IntuitionBase = NULL;
    }
    if (DOSBase)
    {
        CloseLibrary (DOSBase);
        DOSBase = NULL;
    }
}

static void dvb_Shutdown (struct DiffViewClassBase *cb)
{
    if (LineCmpClass)
    {
        RemoveClass (LineCmpClass);
        FreeClass (LineCmpClass);
        LineCmpClass = NULL;
    }
    if (DiffViewClass)
    {
        RemoveClass (DiffViewClass);
        FreeClass (DiffViewClass);
        DiffViewClass = NULL;
    }
    /* FreeScreenDrawInfo / ReleasePen need intuition.library open. */
    DiffPens_ShutdownShared (cb);
    dvb_CloseBases (cb);
}

#ifdef __SASC
ASM int _XCEXIT (void)
{
    return 0;
}
void __regargs __chkabort (void)
{
}
void __regargs _CXBRK (void)
{
}
#endif

struct DiffViewClassBase *ASM LibInit (
    REG (d0) struct DiffViewClassBase *cb,
    REG (a0) BPTR seglist,
    REG (a6) struct Library *sysbase)
{
    SysBase = sysbase;
    cb->dvb_SegList = seglist;
    do_LibBase = cb;
    DiffPens_InitLibrary (cb);

    DOSBase       = OpenLibrary ("dos.library",       WANTED_LIBVER);
    IntuitionBase = OpenLibrary ("intuition.library", WANTED_LIBVER);
    GfxBase       = OpenLibrary ("graphics.library",  WANTED_LIBVER);
    UtilityBase   = OpenLibrary ("utility.library",   WANTED_LIBVER);

    if (!DOSBase || !IntuitionBase || !GfxBase || !UtilityBase)
    {
        dvb_Shutdown (cb);
        FreeMem ((APTR) ((BYTE *) cb - cb->dvb_Lib.lib_NegSize),
                 cb->dvb_Lib.lib_NegSize + cb->dvb_Lib.lib_PosSize);
        return NULL;
    }

    DiffViewClass = DiffViewCreateClass (cb);
    if (!DiffViewClass)
    {
        dvb_Shutdown (cb);
        FreeMem ((APTR) ((BYTE *) cb - cb->dvb_Lib.lib_NegSize),
                 cb->dvb_Lib.lib_NegSize + cb->dvb_Lib.lib_PosSize);
        return NULL;
    }

    LineCmpClass = LineCmpCreateClass (cb);
    if (!LineCmpClass)
    {
        dvb_Shutdown (cb);
        FreeMem ((APTR) ((BYTE *) cb - cb->dvb_Lib.lib_NegSize),
                 cb->dvb_Lib.lib_NegSize + cb->dvb_Lib.lib_PosSize);
        return NULL;
    }

    return cb;
}

struct DiffViewClassBase *ASM LibOpen (
    REG (a6) struct DiffViewClassBase *cb)
{
    do_LibBase = cb;
    cb->dvb_Lib.lib_OpenCnt++;
    cb->dvb_Lib.lib_Flags &= ~LIBF_DELEXP;
    return cb;
}

BPTR ASM LibClose (REG (a6) struct DiffViewClassBase *cb)
{
    cb->dvb_Lib.lib_OpenCnt--;
    if (cb->dvb_Lib.lib_OpenCnt == 0)
    {
        if (cb->dvb_Lib.lib_Flags & LIBF_DELEXP)
            return LibExpunge (cb);
    }
    return (BPTR) NULL;
}

BPTR ASM LibExpunge (REG (a6) struct DiffViewClassBase *cb)
{
    BPTR seglist;

    if (cb->dvb_Lib.lib_OpenCnt != 0)
    {
        cb->dvb_Lib.lib_Flags |= LIBF_DELEXP;
        return (BPTR) NULL;
    }

    if (LineCmpClass)
    {
        RemoveClass (LineCmpClass);
        if (!FreeClass (LineCmpClass))
        {
            AddClass (LineCmpClass);
            cb->dvb_Lib.lib_Flags |= LIBF_DELEXP;
            return (BPTR) NULL;
        }
        LineCmpClass = NULL;
    }
    if (DiffViewClass)
    {
        RemoveClass (DiffViewClass);
        if (!FreeClass (DiffViewClass))
        {
            AddClass (DiffViewClass);
            cb->dvb_Lib.lib_Flags |= LIBF_DELEXP;
            return (BPTR) NULL;
        }
        DiffViewClass = NULL;
    }
    DiffPens_ShutdownShared (cb);
    dvb_CloseBases (cb);

    seglist = cb->dvb_SegList;
    Remove (&cb->dvb_Lib.lib_Node);
    FreeMem ((APTR) ((BYTE *) cb - cb->dvb_Lib.lib_NegSize),
             cb->dvb_Lib.lib_NegSize + cb->dvb_Lib.lib_PosSize);
    return seglist;
}

Class *ASM GetEngine (REG (a6) struct DiffViewClassBase *cb)
{
    return DiffViewClass;
}

Class *ASM DIFFVIEW_GetClass (REG (a6) struct DiffViewClassBase *cb)
{
    return DiffViewClass;
}

Class *ASM LINECMP_GetClass (REG (a6) struct DiffViewClassBase *cb)
{
    return LineCmpClass;
}

APTR ASM L_CreateDiffObject (REG (a6) struct DiffViewClassBase *cb,
                             REG (a0) struct TagItem *tags)
{
    DV_T(cb, "L_CreateDiffObject");
    DV_TH(cb, "cb", cb);
    DV_TH(cb, "tags", tags);
    return CreateDiffObject (cb, tags);
}

void ASM L_FreeDiffObject (REG (a6) struct DiffViewClassBase *cb,
                           REG (a0) APTR obj)
{
    FreeDiffObject (cb, obj);
}

BOOL ASM L_GetDiffObjectAttr (REG (a6) struct DiffViewClassBase *cb,
                              REG (a0) APTR obj,
                              REG (d0) ULONG attr,
                              REG (a1) APTR storage)
{
    return GetDiffObjectAttr (cb, obj, attr, storage);
}
