#ifndef DIFFVIEW_CLASSBASE_H
#define DIFFVIEW_CLASSBASE_H
/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 amigazen project
 */


#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <utility/tagitem.h>
#include <devices/inputevent.h>
#include <string.h>

#include <clib/compiler-specific.h>
#include <clib/macros.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/utility_protos.h>

#include <pragmas/dos_pragmas.h>
#include <pragmas/exec_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>
#include <pragmas/utility_pragmas.h>

#include <proto/alib.h>

#include <gadgets/scroller.h>

#include "diffviewclass.h"

#define CLASSLIBVERSION  1
#define CLASSLIBREVISION 0

struct DiffViewSharedPens
{
    struct Screen   *dvs_Screen;
    struct DrawInfo *dvs_DrawInfo;
    ULONG            dvs_RefCount;
};

struct DiffViewClassBase
{
    struct Library   dvb_Lib;
    UWORD            dvb_Pad;
    struct IClass   *dvb_DiffViewClass;
    BPTR             dvb_SegList;
    struct Library  *dvb_SysBase;
    struct Library  *dvb_DOSBase;
    struct Library  *dvb_IntuitionBase;
    struct Library  *dvb_GfxBase;
    struct Library  *dvb_UtilityBase;
    struct IClass   *dvb_LineCmpClass;
    /* One ObtainBestPen + DrawInfo set per screen (ADiffViewPens pattern). */
    struct DiffViewSharedPens dvb_SharedPens;
};

#define SysBase       cb->dvb_SysBase
#define DOSBase       cb->dvb_DOSBase
#define IntuitionBase cb->dvb_IntuitionBase
#define GfxBase       cb->dvb_GfxBase
#define UtilityBase   cb->dvb_UtilityBase
#define DiffViewClass cb->dvb_DiffViewClass
#define LineCmpClass  cb->dvb_LineCmpClass

#define ASM       __asm
#define REG(x)    register __ ## x

#include "class_iprotos.h"

#endif
