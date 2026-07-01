	IFND DIFFVIEW_CLASSBASE_I
DIFFVIEW_CLASSBASE_I SET	1

	INCLUDE	"exec/types.i"
	INCLUDE	"exec/libraries.i"
	INCLUDE	"exec/lists.i"

   STRUCTURE DiffViewClassBase,LIB_SIZE
	UWORD	dvb_Pad
	ULONG	dvb_DiffViewClass
	ULONG	dvb_SegList
	ULONG	dvb_SysBase
	ULONG	dvb_DOSBase
	ULONG	dvb_IntuitionBase
	ULONG	dvb_GfxBase
	ULONG	dvb_UtilityBase
	ULONG	dvb_LineCmpClass
   LABEL DiffViewClassBase_SIZEOF

	LIBINIT

	ENDC
