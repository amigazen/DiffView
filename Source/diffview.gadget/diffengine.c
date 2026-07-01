/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2005-2012 Matthias Hertel
 * SPDX-FileCopyrightText: 2026 amigazen project
 *
 * Based on ADiffView/src/diffengine/DiffEngine.cpp by Uwe Rosner
 * in turn derived from https://github.com/mathertel/Diff
 *
 * diffengine.c - Myers diff engine.
 *
 * Ported from ADiffView/src/diffengine/DiffEngine.cpp (Eugene Myers /
 * Matthias Hertel algorithm, Uwe Rosner implementation).
 */

#include <string.h>

#include "classbase.h"
#include "diffengine.h"
#include "Debug.h"

typedef struct LcsFrame
{
    LONG lf_LowerA;
    LONG lf_UpperA;
    LONG lf_LowerB;
    LONG lf_UpperB;
} LcsFrame;

#define LCS_STACK_GROW 32

static void de_ReportProgress(DiffEngine *eng, ULONG percent)
{
    ULONG p;

    if (!eng || !eng->de_ProgressCB)
        return;

    if (percent > 100)
        percent = 100;

    p = eng->de_LastReportedPercent;
    while (p < percent)
    {
        p++;
        eng->de_ProgressCB(p, eng->de_ProgressData);
    }
    eng->de_LastReportedPercent = percent;
}

static void de_AddDiffPosition(struct DiffViewClassBase *cb, DiffEngine *eng,
                               ULONG pos)
{
    ULONG *newPos;
    ULONG newCap;

    if (!eng)
        return;

    if (eng->de_NumDiffPositions >= eng->de_DiffCap)
    {
        newCap = eng->de_DiffCap + 32;
        newPos = (ULONG *) AllocMem(newCap * sizeof(ULONG), MEMF_CLEAR);
        if (!newPos)
            return;
        if (eng->de_DiffPositions)
        {
            memcpy(newPos, eng->de_DiffPositions,
                   eng->de_NumDiffPositions * sizeof(ULONG));
            FreeMem(eng->de_DiffPositions, eng->de_DiffCap * sizeof(ULONG));
        }
        eng->de_DiffPositions = newPos;
        eng->de_DiffCap = newCap;
    }

  /* Store 1-based line numbers per SDK spec. */
    eng->de_DiffPositions[eng->de_NumDiffPositions++] = pos + 1;
}

static void de_Optimize(DiffFile *file)
{
    ULONG dataLength;
    ULONG startPos;
    ULONG endPos;
    DiffLine *startLine;
    DiffLine *endLine;

    if (!file)
        return;

    dataLength = DiffFile_NumLines(file);
    startPos = 0;

    while (startPos < dataLength)
    {
        while (startPos < dataLength)
        {
            startLine = DiffFile_Line(file, startPos);
            if (!startLine || DiffLine_State(startLine) == DLS_NORMAL)
                startPos++;
            else
                break;
        }

        endPos = startPos;
        while (endPos < dataLength)
        {
            startLine = DiffFile_Line(file, startPos);
            if (!startLine || DiffLine_State(startLine) == DLS_NORMAL)
                break;
            endPos++;
        }

        if (endPos < dataLength)
        {
            startLine = DiffFile_Line(file, startPos);
            endLine = DiffFile_Line(file, endPos);
            if (startLine && endLine &&
                DiffLine_Token(startLine) == DiffLine_Token(endLine))
            {
                DiffLine_SetState(endLine, DiffLine_State(startLine));
                DiffLine_SetState(startLine, DLS_NORMAL);
            }
            else
                startPos = endPos;
        }
        else
            break;
    }
}

static void de_SmsGet(DiffEngine *eng, LONG lowerA, LONG upperA,
                     LONG lowerB, LONG upperB, LONG *outA, LONG *outB)
{
    LONG downK;
    LONG upK;
    LONG delta;
    UBYTE oddDelta;
    LONG downOffset;
    LONG upOffset;
    LONG maxD;
    LONG *downData;
    LONG *upData;
    LONG D;
    LONG k;
    LONG x;
    LONG y;

    downK = lowerA - lowerB;
    upK = upperA - upperB;
    delta = (upperA - lowerA) - (upperB - lowerB);
    oddDelta = (delta & 1) ? 1 : 0;
    downOffset = (LONG) eng->de_Max - downK;
    upOffset = (LONG) eng->de_Max - upK;
    maxD = ((upperA - lowerA + upperB - lowerB) / 2) + 1;
    downData = eng->de_DownVector + downOffset;
    upData = eng->de_UpVector + upOffset;

    downData[downK + 1] = lowerA;
    upData[upK - 1] = upperA;

    for (D = 0; D <= maxD; D++)
    {
        if (eng->de_Cancelled)
            return;

        for (k = downK - D; k <= downK + D; k += 2)
        {
            if (k == downK - D)
                x = downData[k + 1];
            else
            {
                x = downData[k - 1] + 1;
                if ((k < downK + D) && (downData[k + 1] >= x))
                    x = downData[k + 1];
            }
            y = x - k;
            while (x < upperA && y < upperB)
            {
                DiffLine *la;
                DiffLine *rb;

                la = DiffFile_Line(eng->de_LeftIn, (ULONG) x);
                rb = DiffFile_Line(eng->de_RightIn, (ULONG) y);
                if (!la || !rb ||
                    DiffLine_Token(la) != DiffLine_Token(rb))
                    break;
                x++;
                y++;
            }
            downData[k] = x;
            if (oddDelta && (upK - D < k) && (k < upK + D))
            {
                if (upData[k] <= downData[k])
                {
                    *outA = downData[k];
                    *outB = downData[k] - k;
                    return;
                }
            }
        }

        for (k = upK - D; k <= upK + D; k += 2)
        {
            if (k == upK + D)
                x = upData[k - 1];
            else
            {
                x = upData[k + 1] - 1;
                if ((k > upK - D) && (upData[k - 1] < x))
                    x = upData[k - 1];
            }
            y = x - k;
            while (x > lowerA && y > lowerB)
            {
                DiffLine *la;
                DiffLine *rb;

                la = DiffFile_Line(eng->de_LeftIn, (ULONG) (x - 1));
                rb = DiffFile_Line(eng->de_RightIn, (ULONG) (y - 1));
                if (!la || !rb ||
                    DiffLine_Token(la) != DiffLine_Token(rb))
                    break;
                x--;
                y--;
            }
            upData[k] = x;
            if (!oddDelta && (downK - D <= k) && (k <= downK + D))
            {
                if (upData[k] <= downData[k])
                {
                    *outA = downData[k];
                    *outB = downData[k] - k;
                    return;
                }
            }
        }
    }

    /* Should not happen; midpoint fallback keeps de_Lcs from spinning. */
    *outA = lowerA + ((upperA - lowerA) / 2);
    if (*outA <= lowerA && (upperA - lowerA) > 1)
        *outA = lowerA + 1;
    if (*outA >= upperA && upperA > lowerA)
        *outA = upperA - 1;
    *outB = lowerB + ((upperB - lowerB) / 2);
    if (*outB <= lowerB && (upperB - lowerB) > 1)
        *outB = lowerB + 1;
    if (*outB >= upperB && upperB > lowerB)
        *outB = upperB - 1;
}

static void de_Lcs(struct DiffViewClassBase *cb, DiffEngine *eng)
{
    LcsFrame *stack;
    ULONG stackCap;
    ULONG stackTop;
    LcsFrame f;
    LONG a1;
    LONG a2;
    LONG b1;
    LONG b2;
    LONG splitA;
    LONG splitB;
    DiffLine *line;

    stackCap = LCS_STACK_GROW;
    stack = (LcsFrame *) AllocMem(stackCap * sizeof(LcsFrame), MEMF_CLEAR);
    if (!stack)
        return;

    stackTop = 0;
    stack[stackTop].lf_LowerA = 0;
    stack[stackTop].lf_UpperA = (LONG) DiffFile_NumLines(eng->de_LeftIn);
    stack[stackTop].lf_LowerB = 0;
    stack[stackTop].lf_UpperB = (LONG) DiffFile_NumLines(eng->de_RightIn);
    stackTop++;

    while (stackTop > 0)
    {
        stackTop--;
        f = stack[stackTop];

        if ((eng->de_NotifyIncrement > 0) &&
            (f.lf_LowerA > (LONG) eng->de_NextNotifyPosition))
        {
            if (eng->de_Cancelled)
                break;
            eng->de_Percent += eng->de_PercentIncrement;
            eng->de_NextNotifyPosition += eng->de_NotifyIncrement;
            de_ReportProgress(eng, eng->de_Percent);
        }

        a1 = f.lf_LowerA;
        a2 = f.lf_UpperA;
        b1 = f.lf_LowerB;
        b2 = f.lf_UpperB;

        while (a1 < a2 && b1 < b2)
        {
            DiffLine *la;
            DiffLine *rb;

            la = DiffFile_Line(eng->de_LeftIn, (ULONG) a1);
            rb = DiffFile_Line(eng->de_RightIn, (ULONG) b1);
            if (!la || !rb ||
                DiffLine_Token(la) != DiffLine_Token(rb))
                break;
            a1++;
            b1++;
        }

        while (a1 < a2 && b1 < b2)
        {
            DiffLine *la;
            DiffLine *rb;

            la = DiffFile_Line(eng->de_LeftIn, (ULONG) (a2 - 1));
            rb = DiffFile_Line(eng->de_RightIn, (ULONG) (b2 - 1));
            if (!la || !rb ||
                DiffLine_Token(la) != DiffLine_Token(rb))
                break;
            a2--;
            b2--;
        }

        if (a1 == a2)
        {
            while (b1 < b2)
            {
                line = DiffFile_Line(eng->de_RightIn, (ULONG) b1);
                if (line)
                {
                    DiffLine_SetState(line, DLS_ADDED);
                    eng->de_NumInserted++;
                }
                b1++;
            }
            continue;
        }

        if (b1 == b2)
        {
            while (a1 < a2)
            {
                line = DiffFile_Line(eng->de_LeftIn, (ULONG) a1);
                if (line)
                {
                    DiffLine_SetState(line, DLS_DELETED);
                    eng->de_NumDeleted++;
                }
                a1++;
            }
            continue;
        }

        if (eng->de_Cancelled)
            break;

        splitA = 0;
        splitB = 0;
        de_SmsGet(eng, a1, a2, b1, b2, &splitA, &splitB);
        if (eng->de_Cancelled)
            break;

        if (splitA <= a1 && splitB <= b1)
        {
            splitA = a1 + 1;
            splitB = b1 + 1;
            if (splitA >= a2 && a2 > a1)
                splitA = a2 - 1;
            if (splitB >= b2 && b2 > b1)
                splitB = b2 - 1;
        }

        if (stackTop + 2 > stackCap)
        {
            LcsFrame *newStack;
            ULONG newCap;

            newCap = stackCap + LCS_STACK_GROW;
            newStack = (LcsFrame *) AllocMem(newCap * sizeof(LcsFrame),
                                             MEMF_CLEAR);
            if (!newStack)
                break;
            memcpy(newStack, stack, stackCap * sizeof(LcsFrame));
            FreeMem(stack, stackCap * sizeof(LcsFrame));
            stack = newStack;
            stackCap = newCap;
        }

        stack[stackTop].lf_LowerA = splitA;
        stack[stackTop].lf_UpperA = a2;
        stack[stackTop].lf_LowerB = splitB;
        stack[stackTop].lf_UpperB = b2;
        stackTop++;

        stack[stackTop].lf_LowerA = a1;
        stack[stackTop].lf_UpperA = splitA;
        stack[stackTop].lf_LowerB = b1;
        stack[stackTop].lf_UpperB = splitB;
        stackTop++;
    }

    if (stack)
        FreeMem(stack, stackCap * sizeof(LcsFrame));
}

/*
 * Output DiffFiles outlive the temporary input files, which are freed at the
 * end of CreateDiffObject. Copy line text and line numbers into owned
 * allocations; InitLinked would leave pointers that double-free on teardown.
 */
static DiffLine *de_NewOutputLine(struct DiffViewClassBase *cb, DiffLine *src,
                                  UBYTE state)
{
    DiffLine *out;
    STRPTR textCopy;
    STRPTR numCopy;
    ULONG textLen;

    (void) cb;

    if (!src)
        return NULL;

    textCopy = NULL;
    numCopy = NULL;
    textLen = DiffLine_TextLen(src);

    textCopy = (STRPTR) AllocMem(textLen + 1, MEMF_CLEAR);
    if (!textCopy)
        return NULL;
    if (textLen)
        memcpy(textCopy, DiffLine_Text(src), textLen);
    textCopy[textLen] = '\0';

    if (src->dl_LineNumText)
    {
        numCopy = (STRPTR) AllocMem(strlen(src->dl_LineNumText) + 1, MEMF_CLEAR);
        if (!numCopy)
        {
            FreeMem(textCopy, textLen + 1);
            return NULL;
        }
        strcpy(numCopy, src->dl_LineNumText);
    }

    out = (DiffLine *) AllocMem(sizeof(DiffLine), MEMF_CLEAR);
    if (!out)
    {
        if (numCopy)
            FreeMem(numCopy, strlen(numCopy) + 1);
        FreeMem(textCopy, textLen + 1);
        return NULL;
    }

    DiffLine_Init(out, textCopy, 0);
    DiffLine_SetState(out, state);
    if (numCopy)
        DiffLine_SetLineNumText(out, numCopy);

    return out;
}

static void de_PopulateOutput(struct DiffViewClassBase *cb, DiffEngine *eng)
{
    ULONG lineA;
    ULONG lineB;
    DiffLine *pA;
    DiffLine *pB;
    ULONG idx;
    UBYTE blockAdded;

    lineA = 0;
    lineB = 0;
    eng->de_NumInserted = 0;
    eng->de_NumDeleted = 0;
    eng->de_NumChanged = 0;

    while (lineA < DiffFile_NumLines(eng->de_LeftIn) ||
           lineB < DiffFile_NumLines(eng->de_RightIn))
    {
        while (lineA < DiffFile_NumLines(eng->de_LeftIn) &&
               lineB < DiffFile_NumLines(eng->de_RightIn))
        {
            pA = DiffFile_Line(eng->de_LeftIn, lineA);
            pB = DiffFile_Line(eng->de_RightIn, lineB);
            if (!pA || !pB)
                break;
            if (DiffLine_State(pA) != DLS_NORMAL ||
                DiffLine_State(pB) != DLS_NORMAL)
                break;

            {
                DiffLine *outA;
                DiffLine *outB;

                outA = de_NewOutputLine(cb, pA, DLS_NORMAL);
                outB = de_NewOutputLine(cb, pB, DLS_NORMAL);
                if (!outA || !outB)
                {
                    if (outA)
                    {
                        DiffLine_Free(cb, outA);
                        FreeMem(outA, sizeof(DiffLine));
                    }
                    if (outB)
                    {
                        DiffLine_Free(cb, outB);
                        FreeMem(outB, sizeof(DiffLine));
                    }
                    return;
                }
                DiffFile_AddLine(cb, eng->de_LeftOut, outA);
                DiffFile_AddLine(cb, eng->de_RightOut, outB);
            }
            lineA++;
            lineB++;
        }

        blockAdded = 0;
        while (lineA < DiffFile_NumLines(eng->de_LeftIn) &&
               lineB < DiffFile_NumLines(eng->de_RightIn))
        {
            pA = DiffFile_Line(eng->de_LeftIn, lineA);
            pB = DiffFile_Line(eng->de_RightIn, lineB);
            if (!pA || !pB)
                break;
            if (DiffLine_State(pA) == DLS_NORMAL ||
                DiffLine_State(pB) == DLS_NORMAL)
                break;

            {
                DiffLine *outA;
                DiffLine *outB;

                outA = de_NewOutputLine(cb, pA, DLS_CHANGED);
                outB = de_NewOutputLine(cb, pB, DLS_CHANGED);
                if (!outA || !outB)
                {
                    if (outA)
                    {
                        DiffLine_Free(cb, outA);
                        FreeMem(outA, sizeof(DiffLine));
                    }
                    if (outB)
                    {
                        DiffLine_Free(cb, outB);
                        FreeMem(outB, sizeof(DiffLine));
                    }
                    return;
                }
                idx = DiffFile_NumLines(eng->de_LeftOut);
                DiffFile_AddLine(cb, eng->de_LeftOut, outA);
                DiffFile_AddLine(cb, eng->de_RightOut, outB);
                if (!blockAdded)
                {
                    eng->de_NumChanged++;
                    de_AddDiffPosition(cb, eng, idx);
                    blockAdded = 1;
                }
            }
            lineA++;
            lineB++;
        }

        blockAdded = 0;
        while (lineA < DiffFile_NumLines(eng->de_LeftIn) &&
               (lineB >= DiffFile_NumLines(eng->de_RightIn) ||
                DiffLine_State(DiffFile_Line(eng->de_LeftIn, lineA)) != DLS_NORMAL))
        {
            pA = DiffFile_Line(eng->de_LeftIn, lineA);
            if (!pA)
                break;
            {
                DiffLine *outA;

                outA = de_NewOutputLine(cb, pA, DLS_DELETED);
                if (!outA)
                    return;
                idx = DiffFile_NumLines(eng->de_LeftOut);
                DiffFile_AddLine(cb, eng->de_LeftOut, outA);
                DiffFile_AddEmptyLine(cb, eng->de_RightOut);
                if (!blockAdded)
                {
                    eng->de_NumDeleted++;
                    de_AddDiffPosition(cb, eng, idx);
                    blockAdded = 1;
                }
            }
            lineA++;
        }

        blockAdded = 0;
        while (lineB < DiffFile_NumLines(eng->de_RightIn) &&
               (lineA >= DiffFile_NumLines(eng->de_LeftIn) ||
                DiffLine_State(DiffFile_Line(eng->de_RightIn, lineB)) != DLS_NORMAL))
        {
            pB = DiffFile_Line(eng->de_RightIn, lineB);
            if (!pB)
                break;
            {
                DiffLine *outB;

                DiffFile_AddEmptyLine(cb, eng->de_LeftOut);
                outB = de_NewOutputLine(cb, pB, DLS_ADDED);
                if (!outB)
                    return;
                idx = DiffFile_NumLines(eng->de_RightOut);
                DiffFile_AddLine(cb, eng->de_RightOut, outB);
                if (!blockAdded)
                {
                    eng->de_NumInserted++;
                    de_AddDiffPosition(cb, eng, idx);
                    blockAdded = 1;
                }
            }
            lineB++;
        }
    }
}

void DiffEngine_Init(DiffEngine *eng)
{
    if (!eng)
        return;
    eng->de_LeftIn = NULL;
    eng->de_RightIn = NULL;
    eng->de_LeftOut = NULL;
    eng->de_RightOut = NULL;
    eng->de_DiffPositions = NULL;
    eng->de_NumDiffPositions = 0;
    eng->de_DiffCap = 0;
    eng->de_DownVector = NULL;
    eng->de_UpVector = NULL;
    eng->de_Max = 0;
    eng->de_NumInserted = 0;
    eng->de_NumDeleted = 0;
    eng->de_NumChanged = 0;
    eng->de_ProgressCB = NULL;
    eng->de_ProgressData = NULL;
    eng->de_Cancelled = 0;
    eng->de_LastReportedPercent = 0;
}

void DiffEngine_Free(struct DiffViewClassBase *cb, DiffEngine *eng)
{
    if (!eng)
        return;
    if (eng->de_DiffPositions)
        FreeMem(eng->de_DiffPositions, eng->de_DiffCap * sizeof(ULONG));
    if (eng->de_DownVector)
        FreeMem(eng->de_DownVector, (2 * eng->de_Max + 2) * sizeof(LONG));
    if (eng->de_UpVector)
        FreeMem(eng->de_UpVector, (2 * eng->de_Max + 2) * sizeof(LONG));
    eng->de_DiffPositions = NULL;
    eng->de_DownVector = NULL;
    eng->de_UpVector = NULL;
}

BOOL DiffEngine_Compare(struct DiffViewClassBase *cb, DiffEngine *eng,
                        DiffFile *leftIn, DiffFile *rightIn,
                        DiffFile *leftOut, DiffFile *rightOut,
                        DIFFOBJPROGCB progCB, APTR progData)
{
    ULONG numLines;
    ULONG vecSize;

    if (!cb || !eng || !leftIn || !rightIn || !leftOut || !rightOut)
        return FALSE;

    eng->de_LeftIn = leftIn;
    eng->de_RightIn = rightIn;
    eng->de_LeftOut = leftOut;
    eng->de_RightOut = rightOut;
    eng->de_ProgressCB = progCB;
    eng->de_ProgressData = progData;
    eng->de_Cancelled = 0;

    numLines = DiffFile_NumLines(leftIn) + DiffFile_NumLines(rightIn) + 1;
    eng->de_Max = numLines;
    vecSize = (2 * numLines + 2) * sizeof(LONG);
    eng->de_DownVector = (LONG *) AllocMem(vecSize, MEMF_CLEAR);
    eng->de_UpVector = (LONG *) AllocMem(vecSize, MEMF_CLEAR);
    if (!eng->de_DownVector || !eng->de_UpVector)
        return FALSE;

    eng->de_NotifyIncrement = DiffFile_NumLines(leftIn) / 18;
    if (eng->de_NotifyIncrement < 1)
        eng->de_NotifyIncrement = 1;
    eng->de_PercentIncrement = 90 / 18;
    eng->de_NextNotifyPosition = eng->de_NotifyIncrement;
    eng->de_Percent = 0;
    de_ReportProgress(eng, 0);

    DV_T(cb, "de_Lcs start");
    de_Lcs(cb, eng);
    DV_T(cb, "de_Lcs done");
    if (eng->de_Cancelled)
        return FALSE;

    de_ReportProgress(eng, 90);
    de_Optimize(leftIn);
    de_Optimize(rightIn);
    de_ReportProgress(eng, 95);
    de_PopulateOutput(cb, eng);
    de_ReportProgress(eng, 100);

    return (BOOL) (!eng->de_Cancelled);
}
