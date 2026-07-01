VERSION		EQU	1
REVISION	EQU	0
DATE	MACRO
		dc.b	'30.6.26'
	ENDM
VERS	MACRO
		dc.b	'diffview 1.0'
	ENDM
VSTRING	MACRO
		dc.b	'diffview.gadget 1.0 (30.6.26) Side-by-side file diff viewer',13,10,0
	ENDM
VERSTAG	MACRO
		dc.b	0,'$VER: diffview.gadget 1.0 (30.6.26)',0
	ENDM
