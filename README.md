# DiffView

This is **DiffView** with **`diffview.gadget`** from the amigazen project is a BOOPSI class
library for **AmigaOS 3.x** that delivers the same contract as the AmigaOS 4 gadget `diffview.gadget`, while porting rendering, selection, clipboard, pen management, and the Myers diff engine from
[ADiffView](https://github.com/rosneru/ADiffView). 

## [amigazen project](http://www.amigazen.com)

*A web, suddenly*

*Forty years meditation*

*Minds awaken, free*

**amigazen project** is using modern software development tools and methods to update and rerelease classic Amiga open source software. Projects include a new AWeb, a new Amiga Python 2, and the ToolKit project — a universal SDK for Amiga development. All *amigazen project* releases are guaranteed to build against the ToolKit standard so that anyone can download and begin contributing straightaway without having to tailor the toolchain for their own setup.

This implementation of `diffview.gadget` is a new work of the amigazen project, released under the Apache License 2.0 (see [LICENSE.md](LICENSE.md)) with rendering, selection, clipboard, pen management, and related logic ported from [ADiffView](https://github.com/rosneru/ADiffView) by Uwe Rosner (Copyright 2024, Apache-2.0). The Myers diff engine is ported from ADiffView's `DiffEngine.cpp`, which incorporates third-party code from [mathertel/Diff](https://github.com/mathertel/Diff) (BSD-3-Clause). The public API surface is designed to be **contract compatible** with the AmigaOS 4 `diffview.gadget` interface documented in `diffview.doc`.

The amigazen project philosophy is based on openness:

*Open* to anyone and everyone — *Open* source and free for all — *Open* your mind and create!

PRs for all projects are gratefully received at [GitHub](https://github.com/amigazen/). While the focus now is on classic 68k software, it is intended that all amigazen project releases can be ported to other Amiga-like systems including AROS and MorphOS where feasible.

## History

**ADiffView** was created by **Uwe Rosner** as a graphical ASCII file compare and diff
viewer for AmigaOS 3.0 and later. Distributed on Aminet and MorphOS Storage (version
2.6, June 2024), it compares two text files side by side with colour-coded line states,
an overview strip, intra-line comparison, keyboard navigation between differences, text
selection, and clipboard copy. Its diff engine uses Eugene Myers' algorithm (via
third-party code documented in ADiffView's `LICENSE-3RD-PARTY` file).

ADiffView is a complete application written in C++. The amigazen port extracts the
rendering and interaction logic into reusable BOOPSI classes so other programs can embed
the same presentation without launching ADiffView itself.

Separately, **AmigaOS 4** defined a **`diffview.gadget`** BOOPSI interface — class names,
tags, `DiffObject` construction, and library vectors — so OS4 applications could share one
diff widget implementation. This classic Amiga port offers the same **published API contract**. 

## The side-by-side diff pattern

A file diff is not a single string comparison. Useful viewers build an **aligned line
model**: each row in the left and right panes corresponds to the same logical position
in the merged diff, with padding blank lines where one side has an insertion or deletion.
Within a changed line, character-level highlighting shows exactly which columns differ.

ADiffView established the interaction model that this gadget preserves:

| Role | What you see | DiffView behaviour |
|------|--------------|-------------------|
| **Unchanged line** | Same text both sides | `DLS_NORMAL` — default text colour |
| **Changed line** | Same line number, different text | `DLS_CHANGED` — old/new highlight colours |
| **Deleted line** | Text left, blank right | `DLS_DELETED` — left pane only |
| **Added line** | Blank left, text right | `DLS_ADDED` — right pane only |
| **Whitespace-only** | Text matches after ignoring leading space/tab | `DLS_WS_ONLY` when `DIFFOBJECT_WsUnimportant` is set |
| **Current line** | Cursor row across panes | `DIFFVIEW_CurrentLine`; drives `linecmp.gadget` |
| **Overview** | Colour strip above panes | Click or drag to jump; encodes diff density |
| **Line compare** | Old vs new text of current line | `linecmp.gadget` below the main panes |

Three input modes for `CreateDiffObject()` map to common toolchains:

- **Two memory buffers** — pass `DIFFOBJECT_OldFile` / `OldFileLen` and `NewFile` /
  `NewFileLen`; the library runs `DiffEngine_Compare()` (Myers LCS) and builds aligned
  output files
- **Unified diff + one file** — pass `DIFFOBJECT_Diffs` / `DiffsLen` plus either the old
  or new buffer; `DiffUnified_Build()` reconstructs the paired panes per `diffview.doc`
- **Pre-aligned output** — supply ready-made left/right line sets when an external tool
  has already computed the diff

Unlike piping `diff` output to a Shell window, the gadget keeps **line numbers**,
**horizontal scroll**, **tab expansion**, and **selection geometry** in sync across both
panes and the overview.

## About DiffView

`diffview.gadget` is a **BOOPSI class library** that embeds a full graphical diff viewer
in any Intuition application. A companion **`linecmp.gadget`** shows the old and new
versions of the current line stacked vertically. A **`DiffObject`** holds the aligned
left/right `DiffFile` data, diff-position index, and optional error state — created once,
attached to the gadget, freed when done.

| Concern | Component |
| ------- | --------- |
| Myers diff / alignment | `diffengine.c` — LCS, progress callback, cancellation |
| Unified diff import | `diffunified.c` — parse `diff -u` output against one side |
| Line model | `difffile.c`, `diffline.c` — per-line state, tab-aware columns |
| Rendering | `diffrender.c` — panes, header, overview strip, status bar |
| Colour pens | `diffpens.c` — shared screen pens, tag-driven palette |
| Selection | `diffselect.c` — drag selection per pane, character granularity |
| Clipboard | `diffclip.c` — copy selected text (Ctrl+C) |
| Main gadget | `DiffViewGClass.c` — layout, scrollers, input, navigation |
| Line compare | `LineCmpGClass.c` — single-line old/new display |
| Library API | `diffobject.c`, `classbase.c` — `CreateDiffObject()` and class init |

**DiffView** comprises:

- **`DiffView`** - visual file comparison utility that makes use of diffview.gadget
- **`diffview.gadget`** — BOOPSI library with `diffview.gadget` and `linecmp.gadget` classes
- **`DiffViewTest`** (`Source/test/`) — windowed harness loading two files or built-in demo buffers
- **SDK**
  - Public headers under `Source/include/gadgets/` and `SDK/Include_h/gadgets/`
  - `diffview.h` — `DIFFVIEW_GetClass()`, `LINECMP_GetClass()`, `CreateDiffObject()`
  - `diffview_tags.h` — gadget tags, `DiffObject` tags, error codes, `DiffPosition`

### Typical client session

```c
DiffViewBase = OpenLibrary("diffview.gadget", 0);

diffObj = CreateDiffObjectTags(
    DIFFOBJECT_OldFile,     (ULONG) oldBuf,
    DIFFOBJECT_OldFileLen,  oldLen,
    DIFFOBJECT_NewFile,     (ULONG) newBuf,
    DIFFOBJECT_NewFileLen,  newLen,
    DIFFOBJECT_OldFileName, (ULONG) "left.txt",
    DIFFOBJECT_NewFileName, (ULONG) "right.txt",
    DIFFOBJECT_WsUnimportant, TRUE,
    TAG_END);

diffGad = NewObject(DIFFVIEW_GetClass(), NULL,
    GA_Left, 10, GA_Top, 20,
    GA_Width, 600, GA_Height, 300,
    DIFFVIEW_DiffObject, (ULONG) diffObj,
    DIFFVIEW_Screen,     (ULONG) screen,
    DIFFVIEW_Font,       (ULONG) font,
    DIFFVIEW_LineCmp,    (ULONG) lineCmpGad,
    DIFFVIEW_VertScroller, (ULONG) vScroll,
    DIFFVIEW_HorizScroller, (ULONG) hScroll,
    TAG_END);

/* … event loop … */

FreeDiffObject(diffObj);
CloseLibrary(DiffViewBase);
```

Vertical and horizontal scrollers (typically `scroller.gadget` objects) attach via tags
and stay synchronised with `DIFFVIEW_ViewOffsetTop` / `ViewOffsetLeft`.

### Keyboard navigation

When the diff gadget is active:

| Key | Action |
|-----|--------|
| Up / Down | Scroll one line |
| Left / Right | Scroll horizontally |
| Space / Backspace | Page down / page up |
| `-` / `0` (top-row) | Next / previous difference |
| Home / End | Top / bottom of file |
| Help | Jump to top |
| Ctrl+C | Copy selection to clipboard |

## Contact

- At GitHub https://github.com/amigazen/DiffView/
- On the web at http://www.amigazen.com/ (Amiga browser compatible)
- Or email toolkit@amigazen.com

## Acknowledgements

**ADiffView** was created by **Uwe Rosner** (Copyright 2024, Apache-2.0). Source files
ported from [ADiffView](https://github.com/rosneru/ADiffView) carry a
`Based on ADiffView by Uwe Rosner` notice in their headers. ADiffView remains the
authoritative standalone compare application for AmigaOS 3.x; this project reuses its
presentation and engine logic as a BOOPSI library.

The Myers diff engine incorporates code from [mathertel/Diff](https://github.com/mathertel/Diff)
(BSD-3-Clause, Copyright Matthias Hertel). See [LICENSE.md](LICENSE.md) for full
attribution and the AmigaOS 4 API contract note.

*Amiga* is a trademark of **Amiga Inc**.
