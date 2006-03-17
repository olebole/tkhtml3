/*
 * htmltable.c ---
 *
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static const char rcsid[] = "$Id: htmltable.c,v 1.63 2006/03/17 15:47:10 danielk1977 Exp $";

#include "htmllayout.h"

#define LOG if (pLayout->pTree->options.logcmd && !pLayout->minmaxTest)

struct TableCell {
    BoxContext box;
    int startrow;
    int finrow;
    int colspan;
    HtmlNode *pNode;
};
typedef struct TableCell TableCell;

/*
 * Structure used whilst laying out tables. See HtmlTableLayout().
 */
struct TableData {
    LayoutContext *pLayout;
    int nCol;                /* Total number of columns in table */
    int nRow;                /* Total number of rows in table */
    int *aMaxWidth;          /* Maximum width of each column */
    int *aMinWidth;          /* Minimum width of each column */
    int border_spacing;      /* Pixel value of 'border-spacing' property */
    int availablewidth;      /* Width available between margins for table */

    int *aWidth;             /* Actual width of each column  */
    int *aY;                 /* Top y-coord for each row+1, wrt table box */
    TableCell *aCell;

    int row;                 /* Current row */
    int y;                   /* y-coord to draw at */
    int x;                   /* x-coord to draw at */
    BoxContext *pBox;        /* Box to draw into */
};
typedef struct TableData TableData;

typedef int (CellCallback)(HtmlNode *, int, int, int, int, void *);
typedef int (RowCallback)(HtmlNode *, int, void *);

static int tableIterate(
    HtmlNode*, 
    CellCallback,
    RowCallback,
    void*
);

static CellCallback tableColWidthSingleSpan;
static int tableColWidthMultiSpan(HtmlNode *, int, int, int, int, void *);
static int tableCountCells(HtmlNode *, int, int, int, int, void *);
static int tableDrawCells(HtmlNode *, int, int, int, int, void *);
static int tableDrawRow(HtmlNode *, int, void *);
static void tableCalculateCellWidths(TableData *, int);

#define DISPLAY(pV) (pV ? pV->eDisplay : CSS_CONST_INLINE)

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetWidth --
 * 
 *     Return the value of the 'width' property for a given node.
 *
 *     This function also handles the 'max-width' and 'min-width'
 *     properties. If there is no 'width' attribute and the default value
 *     supplied as the fourth argument is greater than zero, then the
 *     'min-width' and 'max-width' properties are taken into account when
 *     figuring out the return value.
 * 
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
nodeGetWidth(pNode, pwidth, def, pIsFixed, pIsAuto)
    HtmlNode *pNode;          /* Node */
    int pwidth;               /* Width of the containing block */
    int def;                  /* Default value */
    int *pIsFixed;            /* OUT: True if a pixel width */
    int *pIsAuto;             /* OUT: True if value is "auto" */
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    int iWidth;

    iWidth = PIXELVAL(pV, WIDTH, pwidth);

    if (pIsAuto) {
        *pIsAuto = ((iWidth == PIXELVAL_AUTO) ? 1 : 0);
    }
    if (pIsFixed) {
        *pIsFixed = (PIXELVAL(pV, WIDTH, -1) >= 0); 
    }

    assert(iWidth != PIXELVAL_NONE && iWidth != PIXELVAL_NORMAL);
    if (iWidth == PIXELVAL_AUTO) {
        int iMinWidth = PIXELVAL(pV, MIN_WIDTH, pwidth);
        int iMaxWidth = PIXELVAL(pV, MAX_WIDTH, pwidth);

        iWidth = MAX(def, iMinWidth);
        assert(iMaxWidth != PIXELVAL_AUTO && iMaxWidth != PIXELVAL_NORMAL);
        if (iMaxWidth != PIXELVAL_NONE) {
            iWidth = MIN(def, iMaxWidth);
        }
    }

    return iWidth;
}


/*
 *---------------------------------------------------------------------------
 *
 * tableColWidthSingleSpan --
 *
 *     A tableIterate() callback to calculate the widths of all single
 *     span columns in the table.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableColWidthSingleSpan(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;

    if (colspan == 1) {
        int max;
        int min;
        int w;
        int f = 0;

        int *aMinWidth = pData->aMinWidth;
        int *aMaxWidth = pData->aMaxWidth;
        int *aWidth = pData->aWidth;

        /* See if the cell has an explicitly requested width. */
        w = nodeGetWidth(pNode, pData->availablewidth, 0, &f,0);

        /* And figure out the minimum and maximum widths of the content */
        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);

        if (w && f && w > min && w > aMinWidth[col]) {
            aMinWidth[col] = w;
            aMaxWidth[col] = w;
            aWidth[col] = w;
        } else {
            aMinWidth[col] = MAX(aMinWidth[col], min);
            aMaxWidth[col] = MAX(aMaxWidth[col], max);
            if (w && w > aMinWidth[col]) {
                aWidth[col] = w;
                aMaxWidth[col] = w;
            }

            if (aWidth[col] && aWidth[col] < aMinWidth[col]) {
                aWidth[col] = aMinWidth[col];
            }
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableColWidthMultiSpan --
 *
 *     A tableIterate() callback to calculate the minimum and maximum
 *     widths of multi-span cells, and adjust the minimum and maximum
 *     column widths if required.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableColWidthMultiSpan(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
    if (colspan>1) {
        int max;
        int min;
        int currentmin = 0;
        int currentmax = 0;
        int i;
        int numstretchable = 0;

        for (i=col; i<(col+colspan); i++) {
            currentmin += pData->aMinWidth[i];
            currentmax += pData->aMaxWidth[i];
            if (!pData->aWidth[i]) {
                numstretchable++;
            }
        }
        currentmin += (pData->border_spacing * (colspan-1));
        currentmax += (pData->border_spacing * (colspan-1));

        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);
        if (min>currentmin) {
            int incr = (min-currentmin)/(numstretchable?numstretchable:colspan);
            for (i=col; i<(col+colspan); i++) {
                if (numstretchable==0 || pData->aWidth[i]==0) {
                    pData->aMinWidth[i] += incr;
                    if (pData->aMinWidth[i]>pData->aMaxWidth[i]) {
                        currentmax += (pData->aMinWidth[i]-pData->aMaxWidth[i]);
                        pData->aMaxWidth[i] = pData->aMinWidth[i];
                    }
                }
            }
        }
        if (max>currentmax) {
            int incr = (max-currentmax)/(numstretchable?numstretchable:colspan);
            for (i=col; i<(col+colspan); i++) {
                if (numstretchable==0 || pData->aWidth[i]==0) {
                    pData->aMaxWidth[i] += incr;
                }
            }
        }
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableCountCells --
 *
 *     A callback invoked by the tableIterate() function to figure out
 *     how many columns are in the table.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableCountCells(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
 
    /* For the purporses of figuring out the dimensions of the table, cells
     * with rowspan or colspan of 0 count as 1.
     */
    if (rowspan==0) {
        rowspan = 1;
    }
    if (colspan==0) {
        colspan = 1;
    }

    if (pData->nCol<(col+colspan)) {
        pData->nCol = col+colspan;
    }
    if (pData->nRow<(row+rowspan)) {
        pData->nRow = row+rowspan;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableDrawRow --
 *
 *     This is a tableIterate() 'row callback' used while actually drawing
 *     table data to canvas. See comments above tableDrawCells() for a
 *     description.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableDrawRow(pNode, row, pContext)
    HtmlNode *pNode;
    int row;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
    LayoutContext *pLayout = pData->pLayout;
    int nextrow = row+1;
    int x = pData->border_spacing;
    int x1, y1, x2, y2;
    int i;

    /* Add the background and border for the table-row, if a node exists. A
     * node may not exist if the row is entirely populated by overflow from
     * above. For example in the following document, there is no node for the
     * second row of the table.
     *
     *     <table><tr><td rowspan=2></table>
     */
    if (pNode) {
        x1 = pData->border_spacing;
        y1 = pData->aY[row];
        y2 = pData->aY[nextrow];
        x2 = x1;
        for (i=0; i<pData->nCol; i++) {
            x2 += pData->aWidth[i];
        }
        x2 += ((pData->nCol - 1) * pData->border_spacing);
        borderLayout(pLayout, pNode, pData->pBox, x1, y1, x2, y2);
    }

    for (i=0; i<pData->nCol; i++) {
        TableCell *pCell = &pData->aCell[i];
        if (pCell->finrow==nextrow) {
            int x1, y1, x2, y2;
            int y;
            int k;

            x1 = x;
            x2 = x1;
            for (k=i; k<(i+pCell->colspan); k++) {
                x2 += pData->aWidth[k];
            }
            x2 += ((pCell->colspan-1) * pData->border_spacing);
            y1 = pData->aY[pCell->startrow];
            y2 = pData->aY[pCell->finrow] - pData->border_spacing;

            borderLayout(pLayout, pCell->pNode, pData->pBox, x1, y1, x2, y2);

            /* Todo: The formulas for the various vertical alignments below
             *       only work if the top and bottom borders of the cell
             *       are of the same thickness. Same goes for the padding.
             */
            switch (pCell->pNode->pPropertyValues->eVerticalAlign) {
                case CSS_CONST_TOP:
                case CSS_CONST_BASELINE:
                    y = pData->aY[pCell->startrow];
                    break;
                case CSS_CONST_BOTTOM:
                    y = pData->aY[pCell->finrow] - 
                        pCell->box.height -
                        pData->border_spacing;
                    break;
                default:
                    y = pData->aY[pCell->startrow] + 
                            (y2-y1-pCell->box.height) / 2;
                    break;
            }
            DRAW_CANVAS(&pData->pBox->vc, &pCell->box.vc,x,y,pCell->pNode);
            memset(pCell, 0, sizeof(TableCell));
        }
        x += pData->aWidth[i] + pData->border_spacing;
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableDrawCells --
 *
 *     tableIterate() callback to actually draw cells. Drawing uses two
 *     callbacks. This function is called for each cell in the table
 *     and the tableDrawRow() function above is called after each row has
 *     been completed.
 *
 *     This function draws the cell into the BoxContext at location
 *     aCell[col-number].box  in the TableData struct. The border and
 *     background are not drawn at this stage.
 *
 *     When the tableDrawRow() function is called, it is possible to
 *     determine the height of the row. This is needed before cell contents
 *     can be copied into the table canvas, so that the cell can be
 *     vertically aligned correctly, and so that the cell border and
 *     background match the height of the row they are in.
 * 
 *     Plus a few complications for cells that span multiple rows.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableDrawCells(pNode, col, colspan, row, rowspan, pContext)
    HtmlNode *pNode;
    int col;
    int colspan;
    int row;
    int rowspan;
    void *pContext;
{
    TableData *pData = (TableData *)pContext;
    BoxContext *pBox;
    int i;
    int x = 0;
    int y = 0;
    int belowY;

    /* A rowspan of 0 means the cell spans the remainder of the table
     * vertically.  Similarly, a colspan of 0 means the cell spans the
     * remainder of the table horizontally. 
     */
    if (rowspan<=0) {
        rowspan = (pData->nRow-row);
    }
    if (colspan<=0) {
        colspan = (pData->nCol-col);
    }

    y = pData->aY[row];
    if (y==0) {
        y = pData->border_spacing * (row+1);
        pData->aY[row] = y;
    }

    for (i=0; i<col; i++) {
        x += pData->aWidth[i];
    }
    x += ((col+1) * pData->border_spacing);

    pBox = &pData->aCell[col].box;
    assert (pData->aCell[col].finrow==0);
    pData->aCell[col].finrow = row+rowspan;
    pData->aCell[col].startrow = row;
    pData->aCell[col].pNode = pNode;
    pData->aCell[col].colspan = colspan;

    pBox->pFloat = HtmlFloatListNew();

    pBox->iContaining = pData->aWidth[col];
    for (i=col+1; i<col+colspan; i++) {
        pBox->iContaining += (pData->aWidth[i] + pData->border_spacing);
    }

    HtmlLayoutTableCell(pData->pLayout, pBox, pNode, pBox->iContaining);
    belowY = y + pBox->height + pData->border_spacing;

    assert(row+rowspan < pData->nRow+1);
    pData->aY[row+rowspan] = MAX(pData->aY[row+rowspan], belowY);
    for (i=row+rowspan+1; i<=pData->nRow; i++) {
        pData->aY[i] = MAX(pData->aY[row+rowspan], pData->aY[i]);
    }

    HtmlFloatListDelete(pBox->pFloat);
    pBox->pFloat = 0;

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableIterate --
 *
 *     Helper function for HtmlTableLayout, used to iterate through cells
 *     of the table. For the table below, the iteration order is W, X,
 *     Y, Z.
 *
 *     /-------\
 *     | W | X |       row number = 0
 *     |-------|
 *     | Y | Z |       row number = 1
 *     \-------/
 *
 *     For each cell, the function passed as the second argument is 
 *     invoked. The arguments are a pointer to the <td> or <th> node
 *     that identifies the cell, the column number, the colspan, the row
 *     number, the rowspan, and a copy of the pContext argument passed to
 *     iterateTable().
 *
 * Results:
 *     TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     Whatever xCallback does.
 *
 *---------------------------------------------------------------------------
 */
static int 
tableIterate(pNode, xCallback, xRowCallback, pContext)
    HtmlNode *pNode;                               /* The <table> node */
    int (*xCallback)(HtmlNode *, int, int, int, int, void *);  /* Callback */
    int (*xRowCallback)(HtmlNode *, int, void *);  /* Row Callback */
    void *pContext;                                /* pContext of callbacks */
{
    int row = 0;
    int i;
    int maxrow = 0;

    /* The following two variables are used to keep track of cells that
     * span multiple rows. The array aRowSpan is dynamically allocated as
     * needed and freed before this function returns. The allocated size
     * of aRowSpan is stored in nRowSpan.
     * 
     * When iterating through the columns in a row (i.e. <th> or <td> tags
     * that are children of a <tr>) if a table cell with a rowspan greater
     * than 1 is encountered, then aRowSpan[<col-number>] is set to
     * rowspan.
     */
    int nRowSpan = 0;        /* Current allocated size of aRowSpans */
    int *aRowSpan = 0;       /* Space to hold row-span data */

    for (i=0; i<HtmlNodeNumChildren(pNode); i++) {
        HtmlNode *pRow = HtmlNodeChild(pNode, i);
        HtmlComputedValues *pV = pRow->pPropertyValues;
        if (DISPLAY(pV) == CSS_CONST_TABLE_ROW) {
            int col = 0;
            int j;
            int k;
            for (j=0; j<HtmlNodeNumChildren(pRow); j++) {
                HtmlNode *pCell = HtmlNodeChild(pRow, j);
                HtmlComputedValues *pV = pCell->pPropertyValues;
                if (DISPLAY(pV) == CSS_CONST_TABLE_CELL) {
                    CONST char *zSpan;
                    int nSpan;
                    int nRSpan;
                    int rc; 
                    int col_ok = 0;

                    /* Set nSpan to the number of columns this cell spans */
                    zSpan = HtmlNodeAttr(pCell, "colspan");
                    nSpan = zSpan?atoi(zSpan):1;
                    if (nSpan<0) {
                        nSpan = 1;
                    }

                    /* Set nRowSpan to the number of rows this cell spans */
                    zSpan = HtmlNodeAttr(pCell, "rowspan");
                    nRSpan = zSpan?atoi(zSpan):1;
                    if (nRSpan<0) {
                        nRSpan = 1;
                    }

                    /* Now figure out what column this cell falls in. The
                     * value of the 'col' variable is where we would like
                     * to place this cell (i.e. just to the right of the
                     * previous cell), but that might change based on cells
                     * from a previous row with a rowspan greater than 1.
                     * If this is true, we shift the cell one column to the
                     * right until the above condition is false.
                     */
                    do {
                        for (k=col; k<(col+nSpan); k++) {
                            if (k<nRowSpan && aRowSpan[k]) break;
                        }
                        if (k==(col+nSpan)) {
                            col_ok = 1;
                        } else {
                            col++;
                        }
                    } while (!col_ok);

                    /* Update the aRowSpan array. It grows here if required. */
                    if (nRSpan!=1) {
                        if (nRowSpan<(col+nSpan)) {
                            int n = col+nSpan;
                            aRowSpan = (int *)HtmlRealloc((char *)aRowSpan, 
                                    sizeof(int)*n);
                            for (k=nRowSpan; k<n; k++) {
                                aRowSpan[k] = 0;
                            }
                            nRowSpan = n;
                        }
                        for (k=col; k<col+nSpan; k++) {
                            aRowSpan[k] = (nRSpan>1?nRSpan:-1);
                        }
                    }

                    maxrow = MAX(maxrow, row+nRSpan-1);
                    rc = xCallback(pCell, col, nSpan, row, nRSpan, pContext);
                    if (rc!=TCL_OK) {
                        HtmlFree((char *)aRowSpan);
                        return rc;
                    }
                    col += nSpan;
                }
            }
            if (xRowCallback) {
                xRowCallback(pRow, row, pContext);
            }
            row++;
            for (k=0; k<nRowSpan; k++) {
                if (aRowSpan[k]) aRowSpan[k]--;
            }
        }
    }

    while (row<=maxrow && xRowCallback) {
        xRowCallback(0, row, pContext);
        row++;
    }

    HtmlFree((char *)aRowSpan);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * tableCalculateCellWidths  --
 *
 *     Decide on some actual widths for the cells, based on the maximum and
 *     minimum widths, the total width of the table and the floating
 *     margins. As far as I can tell, neither CSS nor HTML specify exactly
 *     how to do this. So we use the following approach:
 *
 *     1. Each cell is assigned it's minimum width.  
 *     2. If there are any columns with an explicit width specified as a
 *        percentage, we allocate extra space to them to try to meet these
 *        requests. Explicit widths may mean that the table is not
 *        completely filled.
 *     3. Remaining space is divided up between the cells without explicit
 *        percentage widths. 
 *     
 *     Data structure notes:
 *        * If a column had an explicit width specified in pixels, then the
 *          aWidth[], aMinWidth[] and aMaxWidth[] entries are all set to
 *          this value.
 *        * If a column had an explicit width as a percentage, then the
 *          aMaxWidth[] and aWidth[] entries are set to this value
 *          (converted to pixels, not as a percentage). The aMinWidth entry
 *          is still set to the minimum width required to render the
 *          column.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
tableCalculateCellWidths(pData, width)
    TableData *pData;
    int width;                       /* Total width available for cells */
{
    int i;                           /* Counter variable for small loops */
    int space;                       /* Remaining pixels to allocate */
    int requested;                   /* How much extra space requested */

    int *aTmpWidth;
    int *aWidth = pData->aWidth;
    int *aMinWidth = pData->aMinWidth;
    int *aMaxWidth = pData->aMaxWidth;
    int nCol = pData->nCol;

#ifndef NDEBUG
    for (i=0; i<nCol; i++) {
        assert(!aWidth[i] || aWidth[i] >= aMinWidth[i]);
        assert(!aWidth[i] || aWidth[i] <= aMaxWidth[i]);
        assert(aMinWidth[i] <= aMaxWidth[i]);
    }
#endif

    aTmpWidth = (int *)HtmlAlloc(sizeof(int)*nCol);
    space = width;

    requested = 0;
    for (i=0; i<nCol; i++) {
        aTmpWidth[i] = aMinWidth[i];
        space -= aMinWidth[i];
        if (aWidth[i]) {
            requested += (aWidth[i] - aTmpWidth[i]);
        }
    }
    assert(space>=0);

    if (space<requested) {
        /* This algorithm runs if more space has been requested than is
         * available. i.e. if a table contains two cells with widths of
         * 60%. In this case we only asign extra space to cells that have
         * explicitly requested it.
         */
        for (i=0; i<nCol; i++) {
            if (aWidth[i]) {
                int colreq = (aWidth[i] - aTmpWidth[i]);
                int extra;
 
                if (colreq==requested) {
                    extra = space;
                } else {
                    extra = ((double)space/(double)requested)*(double)colreq;
                }

                space -= extra;
                aTmpWidth[i] += extra;
                requested -= colreq;
            }
        }
        assert(space==0);
    } else {
        /* There is more space available than has been requested. The width
         * of each column is increased as follows:
         *
         * 1. Give every column the extra width it has requested.
         */

        int increase_all_cells = 0;
        requested = 0;
        for (i=0; i<nCol; i++) {
            if (aWidth[i]) {
                space -= (aWidth[i] - aTmpWidth[i]);
                aTmpWidth[i] = aWidth[i];
            }
            assert(aMaxWidth[i]>=aTmpWidth[i]);
            requested += (aMaxWidth[i]-aTmpWidth[i]);
        }
        assert(space>=0);

        if (requested == 0) {
            increase_all_cells = 1;
            requested = nCol;
        }

        for (i=0; i<nCol; i++) {
            int colreq = (aMaxWidth[i] - aTmpWidth[i]) + increase_all_cells;
            int extra;

            if (colreq==requested) {
                extra = space;
            } else {
                extra = ((double)space/(double)requested)*(double)colreq;
            }

            space -= extra;
            aTmpWidth[i] += extra;
            requested -= colreq;
        }

    }

    memcpy(aWidth, aTmpWidth, sizeof(int)*nCol);
    HtmlFree((char *)aTmpWidth);

#ifndef NDEBUG
    for (i=0; i<nCol; i++) {
        assert(aWidth[i] >= aMinWidth[i]);
    }
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableLayout --
 *
 *     Lay out a table node.
 *
 *     Laying out tables largely uses HTML tags directly, instead of
 *     mapping them to CSS using a stylesheet first. This is not ideal, but
 *     the CSS table model is not complete by itself, it relies on the
 *     document language to specify some elements of table structure.
 *     Todo: In the long term, figure out if this can be fixed - either
 *     with CSS3, custom style-sheet syntax, or something I'm not currently
 *     aware of in CSS2.
 *
 *     Note that HTML tags are only used to determine table structure, 
 *     stylesheet rules that apply to table cells are still applied, and
 *     CSS properties assigned to table elements are still respected.
 *     i.e. stuff like "TH {font-weight: bold}" still works.
 *
 *     Todo: List of Html tags/attributes used directly.
 *
 *     This is an incomplete implementation of HTML tables - it does not
 *     support the <col>, <colspan>, <thead>, <tfoot> or <tbody> elements.
 *     Since the parser just ignores tags that we don't know about, this
 *     means that all children of the <table> node should have tag-type
 *     <tr>. Omitting <thead>, <tfoot> and <tbody> is not such a big deal
 *     since it is optional to format these elements differently anyway,
 *     but <col> and <colspan> are fairly important.
 * 
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlTableLayout(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;         /* The node to layout */
{
    HtmlComputedValues *pV = pNode->pPropertyValues;
    int nCol = 0;            /* Number of columns in this table */
    int i;
    int minwidth;            /* Minimum width of entire table */
    int maxwidth;            /* Maximum width of entire table */
    int width;               /* Actual width of entire table */
    int availwidth;          /* Total width available for cells */

    int *aMinWidth = 0;      /* Minimum width for each column */
    int *aMaxWidth = 0;      /* Minimum width for each column */
    int *aWidth = 0;         /* Actual width for each column */
    int *aY = 0;             /* Top y-coord for each row */
    TableCell *aCell = 0;    /* Array of nCol cells used during drawing */
    TableData data;

    memset(&data, 0, sizeof(struct TableData));
    data.pLayout = pLayout;

    pBox->iContaining = MAX(pBox->iContaining, 0);  /* ??? */
    assert(pBox->iContaining>=0);

    assert(pV->eDisplay==CSS_CONST_TABLE);

    /* Read the value of the 'border-spacing' property. 'border-spacing' may
     * not take a percentage value, so there is no need to use PIXELVAL().
     */
    data.border_spacing = pV->iBorderSpacing;

    /* First step is to figure out how many columns this table has.
     * There are two ways to do this - by looking at COL or COLGROUP
     * children of the table, or by counting the cells in each rows.
     * Technically, we should use the first method if one or more COL or
     * COLGROUP elements exist. For now though, always use the second 
     * method.
     */
    tableIterate(pNode, tableCountCells, 0, &data);
    nCol = data.nCol;

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout() "
            "Dimensions are %dx%d",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            data.nCol, data.nRow
        );
    }

    /* Allocate arrays for the minimum and maximum widths of each column */
    aMinWidth = (int *)HtmlAlloc(nCol*sizeof(int));
    memset(aMinWidth, 0, nCol*sizeof(int));
    aMaxWidth = (int *)HtmlAlloc(nCol*sizeof(int));
    memset(aMaxWidth, 0, nCol*sizeof(int));
    aWidth = (int *)HtmlAlloc(nCol*sizeof(int));
    memset(aWidth, 0, nCol*sizeof(int));
    aY = (int *)HtmlClearAlloc((data.nRow+1)*sizeof(int));
    memset(aY, 0, (data.nRow+1)*sizeof(int));
    aCell = (TableCell *)HtmlAlloc(data.nCol*sizeof(TableCell));
    memset(aCell, 0, data.nCol*sizeof(TableCell));

    data.aMaxWidth = aMaxWidth;
    data.aMinWidth = aMinWidth;
    data.aWidth = aWidth;

    /* Figure out the width available for the table between the margins.
     *
     * Todo: Need to take into account padding properties and floats to
     *       figure out the margins.
     */
    data.availablewidth = pBox->iContaining;

    /* Now calculate the minimum and maximum widths of each column. 
     * The first pass only considers cells that span a single column. In
     * this case the min/max width of each column is the maximum of the 
     * min/max widths for all cells in the column.
     * 
     * If the table contains one or more cells that span more than one
     * column, we make a second pass. The min/max widths are increased,
     * if necessary, to account for the multi-column cell. In this case,
     * the width of each column that the cell spans is increased by 
     * the same amount (plus or minus a pixel to account for integer
     * rounding).
     *
     * The minimum and maximum widths for cells take into account the
     * widths of borders (if applicable).
     */
    tableIterate(pNode, tableColWidthSingleSpan, 0, &data);
    tableIterate(pNode, tableColWidthMultiSpan, 0, &data);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Interp *interp = pTree->interp;
        Tcl_Obj *pWidths = Tcl_NewObj();
        int ii;

        /* Log the minimum widths */
        pWidths = Tcl_NewObj();
        Tcl_IncrRefCount(pWidths);
        for (ii = 0; ii < data.nCol; ii++) {
            Tcl_Obj *pInt = Tcl_NewIntObj(data.aMinWidth[ii]);
            Tcl_ListObjAppendElement(interp, pWidths, pInt);
        }
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout()"
            "Cell minimum widths: %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            Tcl_GetString(pWidths)
        );
        Tcl_DecrRefCount(pWidths);

        /* Log the maximum widths */
        pWidths = Tcl_NewObj();
        Tcl_IncrRefCount(pWidths);
        for (ii = 0; ii < data.nCol; ii++) {
            Tcl_Obj *pInt = Tcl_NewIntObj(data.aMaxWidth[ii]);
            Tcl_ListObjAppendElement(interp, pWidths, pInt);
        }
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout()"
            "Cell maximum widths: %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            Tcl_GetString(pWidths)
        );
        Tcl_DecrRefCount(pWidths);
    }

    /* Set variable 'width' to the actual width for the entire table. This
     * is the sum of the widths of the cells plus the border-spacing. Variables
     * minwidth and maxwidth are the minimum and maximum allowable widths for
     * the table based on the min and max widths of the columns.
     *
     * The actual width of the table is based on the following rules, in
     * order of precedence:
     *     * It is never less than minwidth,
     *     * If a width has been specifically requested, via the width
     *       property (or HTML attribute), and it is greater than minwidth,
     *       use the specifically requested width.
     *     * Otherwise use the smaller of maxwidth and the width of the
     *       parent box.
     */

    /* Set minwidth and maxwidth to the minimum and maximum width renderings of
     * the table based on the content.
     */
    minwidth = (nCol+1) * data.border_spacing;
    maxwidth = (nCol+1) * data.border_spacing;
    for (i=0; i<nCol; i++) {
        minwidth += aMinWidth[i];
        maxwidth += aMaxWidth[i];
    }
    assert(maxwidth>=minwidth);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout() "
            "minwidth=%d  maxwidth=%d  containing=%d prescribed-width=%d",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            minwidth, maxwidth, pBox->iContaining, pBox->width
        );
    }

    /* When this function is called, the iContaining has already been set
     * by blockLayout() if there is an explicit 'width'. So we just need to
     * worry about the implicit minimum and maximum width as determined by
     * the table content here.
     */
    if (pBox->width != 0) {
        maxwidth = pBox->width;
    }
    width = MIN(pBox->iContaining, maxwidth);
    width = MAX(minwidth, width);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout()"
            "Actual table width = %d",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            width
        );
    }

    /* Decide on some actual widths for the cells */
    availwidth = width - (nCol+1)*data.border_spacing;
    tableCalculateCellWidths(&data, availwidth);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Interp *interp = pTree->interp;
        Tcl_Obj *pWidths = Tcl_NewObj();
        int ii;

        /* Log the minimum widths */
        pWidths = Tcl_NewObj();
        Tcl_IncrRefCount(pWidths);
        for (ii = 0; ii < data.nCol; ii++) {
            Tcl_Obj *pInt = Tcl_NewIntObj(data.aWidth[ii]);
            Tcl_ListObjAppendElement(interp, pWidths, pInt);
        }
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout()"
            "Actual cell widths: %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            Tcl_GetString(pWidths)
        );
        Tcl_DecrRefCount(pWidths);
    }

    
    /* Now actually draw the cells. */
    data.aY = aY;
    data.aCell = aCell;
    data.pBox = pBox;

    tableIterate(pNode, tableDrawCells, tableDrawRow, &data);

    pBox->height = MAX(pBox->height, data.aY[data.nRow]);
    pBox->width = MAX(pBox->width, width);

    assert(pBox->height < 10000000);
    assert(pBox->width < 10000000);

    HtmlFree((char *)aMinWidth);
    HtmlFree((char *)aMaxWidth);
    HtmlFree((char *)aWidth);
    HtmlFree((char *)aY);
    HtmlFree((char *)aCell);

    return TCL_OK;
}

