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
static const char rcsid[] = "$Id: htmltable.c,v 1.69 2006/04/29 14:35:37 danielk1977 Exp $";

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
    HtmlNode *pNode;         /* <table> node */
    LayoutContext *pLayout;
    int border_spacing;      /* Pixel value of 'border-spacing' property */
    int availablewidth;      /* Width available between margins for table */

    int nCol;                /* Total number of columns in table */
    int nRow;                /* Total number of rows in table */

    int *aMaxWidth;          /* Maximum width of each column */
    int *aMinWidth;          /* Minimum width of each column */
    float *aPercentWidth;    /* Percentage widths of each column */
    int *aExplicitWidth;     /* Explicit widths of each column  */

    int *aWidth;             /* Actual widths of each column  */

    int *aY;                 /* Top y-coord for each row+1, wrt table box */
    TableCell *aCell;

    int row;                 /* Current row */
    int y;                   /* y-coord to draw at */
    int x;                   /* x-coord to draw at */
    BoxContext *pBox;        /* Box to draw into */
};
typedef struct TableData TableData;

/* The two types of callbacks made by tableIterate(). */
typedef int (CellCallback)(HtmlNode *, int, int, int, int, void *);
typedef int (RowCallback)(HtmlNode *, int, void *);

/* Iterate through each cell in each row of the table. */
static int tableIterate(HtmlTree *,HtmlNode*, CellCallback, RowCallback, void*);

/* Count the number of rows/columns in the table */
static CellCallback tableCountCells;

/* Populate the aMinWidth, aMaxWidth, aPercentWidth and aExplicitWidth
 * array members of the TableData structure.
 */
static CellCallback tableColWidthSingleSpan;
static CellCallback tableColWidthMultiSpan;

/* Figure out the actual column widths (TableData.aWidth[]). */
static void tableCalculateCellWidths(TableData *, int, int);

/* A row and cell callback (used together in a single iteration) to draw
 * the table content. All the actual drawing is done here. Everything
 * else is just about figuring out column widths.
 */
static CellCallback tableDrawCells;
static RowCallback tableDrawRow;



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
        HtmlComputedValues *pV = pNode->pPropertyValues;
        BoxProperties box;
        int max;
        int min;
        int req;

        int *aMinWidth       = pData->aMinWidth;
        int *aMaxWidth       = pData->aMaxWidth;
        int *aExplicitWidth  = pData->aExplicitWidth;
        float *aPercentWidth = pData->aPercentWidth;

        /* Figure out the minimum and maximum widths of the content */
        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);
        nodeGetBoxProperties(pData->pLayout, pNode, 0, &box);
        req = pV->iWidth + box.iLeft + box.iRight;

        aMinWidth[col] = MAX(aMinWidth[col], min + box.iLeft + box.iRight);
        aMaxWidth[col] = MAX(aMaxWidth[col], max + box.iLeft + box.iRight);
        
        if (pV->mask & PROP_MASK_WIDTH) {
            /* The computed value of the 'width' property is a percentage */
            float percent_value = ((float)pV->iWidth) / 100.0; 
            aPercentWidth[col] = MAX(aPercentWidth[col], percent_value);
        } else if (req > 0) {
            assert(pV->iWidth >= 0);
            aExplicitWidth[col] = MAX(aExplicitWidth[col], req);
        }
        assert(aMinWidth[col] <= aMaxWidth[col]);
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
 *     Todo: Account for the 'width' property on cells that span multiple
 *           columns.
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
        BoxProperties box;
        int max;
        int min;
        int i;
        // int numstretchable = 0;

        int currentmin;
        int currentmax;
        int minincr;
        int maxincr;

        int *aMinWidth     = pData->aMinWidth;
        int *aMaxWidth     = pData->aMaxWidth;
        // float *aPercentWidth = pData->aPercentWidth;

	/* Calculate the current collective minimum and maximum widths of the
         * spanned columns.
         */
	currentmin = (pData->border_spacing * (colspan-1));
        currentmax = (pData->border_spacing * (colspan-1));
        for (i=col; i<(col+colspan); i++) {
            currentmin += aMinWidth[i];
            currentmax += aMaxWidth[i];
        }

        /* Calculate the maximum and minimum widths of this cell */
        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);
        nodeGetBoxProperties(pData->pLayout, pNode, 0, &box);
        min += box.iLeft + box.iRight;
        max += box.iLeft + box.iRight;

        /* Increment the aMaxWidth[] and aMinWidth[] entries accordingly */
        minincr = MAX((min - currentmin) / colspan, 0);
        maxincr = MAX((max - currentmax) / colspan, 0);
        for (i=col; i<(col+colspan); i++) {
            assert(aMinWidth[i] <= aMaxWidth[i]);
            pData->aMaxWidth[i] += maxincr;
            pData->aMinWidth[i] += minincr;

            /* Account for pixel rounding */
            if ((i+1) == (col+colspan)) {
                aMinWidth[i] += MAX(0, (min-currentmin) - (colspan*minincr));
                aMaxWidth[i] += MAX(0, (max-currentmax) - (colspan*maxincr));
            }

	    /* Todo: If the min-width of the column is now greater than the
	     * max-width, increase the max-width so that it is equal to the
	     * min-width. This means that we may have increased the maximum
	     * widths by too much. Not such a big deal in practice...
             */
	    aMaxWidth[i] = MAX(aMinWidth[i], aMaxWidth[i]);
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
    int x = 0;                             /* X coordinate to draw content */
    int i;                                 /* Column iterator */
    const int mmt = pLayout->minmaxTest;

    /* Add the background and border for the table-row, if a node exists. A
     * node may not exist if the row is entirely populated by overflow from
     * above. For example in the following document, there is no node for the
     * second row of the table.
     *
     *     <table><tr><td rowspan=2></table>
     */
    if (pNode) {
        int x1, y1, w1, h1;           /* Border coordinates */
        x1 = pData->border_spacing;
        y1 = pData->aY[row];
        h1 = pData->aY[nextrow] - pData->aY[row] - pData->border_spacing;
        w1 = 0;
        for (i=0; i<pData->nCol; i++) {
            w1 += pData->aWidth[i];
        }
        w1 += ((pData->nCol - 1) * pData->border_spacing);
        HtmlDrawBox(&pData->pBox->vc, x1, y1, w1, h1, pNode, 0, mmt);
    }

    for (i=0; i<pData->nCol; i++) {
        TableCell *pCell = &pData->aCell[i];

	/* At this point variable x holds the horizontal canvas offset of
         * the outside edge of the cell pCell's left border.
         */
        x += pData->border_spacing;
        if (pCell->finrow == nextrow) {
            BoxProperties box;
            int x1, y1, w1, h1;           /* Border coordinates */
            int y;
            int k;

            HtmlCanvas *pCanvas = &pData->pBox->vc;

            x1 = x;
            y1 = pData->aY[pCell->startrow];
            w1 = 0;
            for (k=i; k<(i+pCell->colspan); k++) {
                w1 += pData->aWidth[k];
            }
            w1 += ((pCell->colspan-1) * pData->border_spacing);
            h1 = pData->aY[pCell->finrow] - pData->border_spacing - y1;
            HtmlDrawBox(pCanvas, x1, y1, w1, h1, pCell->pNode, 0, mmt);
            nodeGetBoxProperties(pLayout, pCell->pNode, 0, &box);

            /* Todo: The formulas for the various vertical alignments below
             *       only work if the top and bottom borders of the cell
             *       are of the same thickness. Same goes for the padding.
             */
            switch (pCell->pNode->pPropertyValues->eVerticalAlign) {
                case CSS_CONST_TOP:
                case CSS_CONST_BASELINE:
                    y = pData->aY[pCell->startrow] + box.iTop;
                    break;
                case CSS_CONST_BOTTOM:
                    y = pData->aY[pCell->finrow] - 
                        pCell->box.height -
                        box.iBottom -
                        pData->border_spacing;
                    break;
                default:
                    y = pData->aY[pCell->startrow];
                    y += (h1 - box.iTop - box.iBottom - pCell->box.height) / 2;
                    y += box.iTop;
                    break;
            }
            DRAW_CANVAS(pCanvas, &pCell->box.vc, x+box.iLeft, y, pCell->pNode);
            memset(pCell, 0, sizeof(TableCell));
        }
        x += pData->aWidth[i];
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
    BoxProperties box;
    int i;
    int x = 0;
    int y = 0;
    int belowY;
    LayoutContext *pLayout = pData->pLayout;

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

    nodeGetBoxProperties(pData->pLayout, pNode, 0, &box);
    pBox->iContaining = pData->aWidth[col] - box.iLeft - box.iRight;

    for (i=col+1; i<col+colspan; i++) {
        pBox->iContaining += (pData->aWidth[i] + pData->border_spacing);
    }

    HtmlLayoutNodeContent(pData->pLayout, pBox, pNode);
    belowY = y + pBox->height + pData->border_spacing + box.iTop + box.iBottom;
    
    LOG {
        HtmlTree *pTree = pLayout->pTree;
        HtmlLog(pTree, "LAYOUTENGINE", "%s tableDrawCells() "
            "containing=%d actual=%d",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            pBox->iContaining, pBox->width
        );
    }

    assert(row+rowspan < pData->nRow+1);
    pData->aY[row+rowspan] = MAX(pData->aY[row+rowspan], belowY);
    for (i=row+rowspan+1; i<=pData->nRow; i++) {
        pData->aY[i] = MAX(pData->aY[row+rowspan], pData->aY[i]);
    }

    return TCL_OK;
}

struct RowIterateContext {
    /* The cell and row callbacks */
    int (*xRowCallback)(HtmlNode *, int, void *);
    int (*xCallback)(HtmlNode *, int, int, int, int, void *);
    ClientData clientData;        /* Client data for the callbacks */

    /* The following two variables are used to keep track of cells that
     * span multiple rows. The array aRowSpan is dynamically allocated as
     * needed and freed before tableIterate() returns. The allocated size
     * of aRowSpan is stored in nRowSpan.
     * 
     * When iterating through the columns in a row (i.e. <th> or <td> tags
     * that are children of a <tr>) if a table cell with a rowspan greater
     * than 1 is encountered, then aRowSpan[<col-number>] is set to
     * rowspan. */
    int nRowSpan;
    int *aRowSpan;

    int iMaxRow;        /* Index of the final row of table */

    int iRow;           /* The current row number (first row is 0) */
    int iCol;           /* The current col number (first row is 0) */
};
typedef struct RowIterateContext RowIterateContext;

static int 
cellIterate(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    RowIterateContext *p = (RowIterateContext *)clientData;

    if (DISPLAY(pNode->pPropertyValues) == CSS_CONST_TABLE_CELL) {
        int nSpan;
        int nRSpan;
        int col_ok = 0;
        char const *zSpan = 0;

        /* Set nSpan to the number of columns this cell spans */
        zSpan = HtmlNodeAttr(pNode, "colspan");
        nSpan = zSpan?atoi(zSpan):1;
        if (nSpan<0) {
            nSpan = 1;
        }

        /* Set nRowSpan to the number of rows this cell spans */
        zSpan = HtmlNodeAttr(pNode, "rowspan");
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
            int k;
            for (k = p->iCol; k < (p->iCol + nSpan); k++) {
                if (k < p->nRowSpan && p->aRowSpan[k]) break;
            }
            if (k == (p->iCol + nSpan)) {
                col_ok = 1;
            } else {
                p->iCol++;
            }
        } while (!col_ok);

        /* Update the p->aRowSpan array. It grows here if required. */
        if (nRSpan!=1) {
            int k;
            if (p->nRowSpan<(p->iCol+nSpan)) {
                int n = p->iCol+nSpan;
                p->aRowSpan = (int *)HtmlRealloc((char *)p->aRowSpan, 
                        sizeof(int)*n);
                for (k=p->nRowSpan; k<n; k++) {
                    p->aRowSpan[k] = 0;
                }
                p->nRowSpan = n;
            }
            for (k=p->iCol; k<p->iCol+nSpan; k++) {
                assert(k < p->nRowSpan);
                p->aRowSpan[k] = (nRSpan>1?nRSpan:-1);
            }
        }

        if (p->xCallback) {
            p->xCallback(pNode, p->iCol, nSpan, p->iRow, nRSpan, p->clientData);
        }
        p->iCol += nSpan;
        p->iMaxRow = MAX(p->iMaxRow, p->iRow + nRSpan - 1);
        return HTML_WALK_DO_NOT_DESCEND;
    }

    /* If the node is not a {display:table-cell} node, then descend. */
    return HTML_WALK_DESCEND;
}

static int 
rowIterate(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    RowIterateContext *p = (RowIterateContext *)clientData;
    if (DISPLAY(pNode->pPropertyValues) == CSS_CONST_TABLE_ROW) {
        int k;
        p->iCol = 0;
        HtmlWalkTree(pTree, pNode, cellIterate, clientData);
        if (p->xRowCallback) {
            p->xRowCallback(pNode, p->iRow, p->clientData);
        }
        p->iRow++;
        for (k=0; k < p->nRowSpan; k++) {
            if (p->aRowSpan[k]) p->aRowSpan[k]--;
        }
        return HTML_WALK_DO_NOT_DESCEND;
    }

    /* If the node is not a {display:table-row} node, then descend. */
    return HTML_WALK_DESCEND;
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
tableIterate(pTree, pNode, xCallback, xRowCallback, pContext)
    HtmlTree *pTree;
    HtmlNode *pNode;                               /* The <table> node */
    int (*xCallback)(HtmlNode *, int, int, int, int, void *);  /* Callback */
    int (*xRowCallback)(HtmlNode *, int, void *);  /* Row Callback */
    void *pContext;                                /* pContext of callbacks */
{
    RowIterateContext sRowContext;
    memset(&sRowContext, 0, sizeof(RowIterateContext));

    sRowContext.xRowCallback = xRowCallback;
    sRowContext.xCallback  = xCallback;
    sRowContext.clientData = (ClientData)pContext;

    HtmlWalkTree(pTree, pNode, rowIterate, (ClientData)&sRowContext);

    while (sRowContext.iRow <= sRowContext.iMaxRow && xRowCallback) {
        xRowCallback(0, sRowContext.iRow, pContext);
        sRowContext.iRow++;
    }
    HtmlFree((char *)sRowContext.aRowSpan);
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
 *     how to do this. 
 *
 *     The inputs to the algorithm are the following quantities for each
 *     column (in arrays TableData.aMinWidth, aMaxWidth, aWidth and
 *     aPercentWidth respectively):
 *
 *         * An explicit pixel width (if 'width' is neither "auto" or a %).
 *         * The minimum content width.
 *         * The maximum content width.
 *         * A percentage pixel width (if 'width is a %).
 *
 *     And:
 *
 *         * The available width,
 *         * Whether or not the 'width' property of the <table> is "auto".
 *
 *     Whether the available width is determined by the 'width' property or by
 *     the width of the containing block (if 'width' is "auto"), the available
 *     width passed as the second parameter to this function does not include
 *     space required for space added due to the 'border-spacing' property.
 *
 *     Widths are assigned to columns by the following procedure:
 *
 *         1. A column that has an explicit width is given that width. If
 *            the explicit width is less than the minimum width, it is
 *            assigned the minimum width instead.
 *
 *         2. Each column that does not have an explicit width is assigned
 *            a percentage width, the greater of:
 *
 *            * (mcw/available-width) * 100%, where mcw is the minimum
 *              content width of the column.
 *            * the specified percentage width, if any.
 *
 *         3. If the sum of the percentages assigned is less than the
 *            proportion of space allocated to nodes without explicit
 *            widths, then increase the % widths of the nodes that have
 *            the width property set to 'auto'. Each is increased in
 *            proportion to ((max-min)/min), where max and min are the
 *            maximum and minimum content widths of the nodes,
 *            respectively.
 *
 *            If the 'width' property of the <table> was not "auto", then
 *            the percentage widths are increased even if that means
 *            exceeding the maximum width for the column.
 *
 *         4. If the sum of the percentages assigned is greater than the
 *            proportion of space allocated to nodes without explicit
 *            widths, then decrease % widths of the nodes that have
 *            specified % widths. Decrease in proportion to
 *            (spec%-min%)/min%, where spec% is the specified percentage,
 *            and min% is the other percentage calculated in step 2.
 *
 *            This step never decreases a percentage width below min%.
 *
 *         5. Assign widths to all columns not assigned widths in step 1
 *            are calculated according to their assigned percentages with
 *            respect to the available width.
 *
 *     Using this algorithm the actual sum of the assigned table widths may
 *     be greater than or less than the value passed as the second
 *     parameter to this function. The actual column widths should be used
 *     to calculate an actual table width - irrespective of the 'width'
 *     attribute - after this function has run.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Modifies the TableData.aWidth[] array to contain actual cell widths.
 *
 *---------------------------------------------------------------------------
 */
static void 
tableCalculateCellWidths(pData, availablewidth, isAuto)
    TableData *pData;
    int availablewidth;    /* Total width available for cells */
    int isAuto;            /* True if the 'width' of the <table> was "auto" */
{
    int i;                    /* Counter variable for small loops */
    int explicit;             /* Pixels used by explicit + minimum widths. */
    int nCol = pData->nCol;
    LayoutContext *pLayout = pData->pLayout;

    /* Local handles for the input arrays */
    int   *aWidth = pData->aWidth;
    int   *aMinWidth = pData->aMinWidth;
    int   *aMaxWidth = pData->aMaxWidth;
    float *aPercentWidth = pData->aPercentWidth;
    int   *aExplicitWidth = pData->aExplicitWidth;

    static const int PIXELVAL_PERCENT = -1; 
    assert(PIXELVAL_AUTO != PIXELVAL_PERCENT);

    /* Allocate either the explicit or minimum width to each cell.  The
     * total number of pixels allocated by this phase is stored in variable
     * 'explicit'.  */
    explicit = 0;
    for (i = 0; i < nCol; i++) {
        assert(aWidth[i] == 0);
        assert(aExplicitWidth[i] >= 0 || aExplicitWidth[i] == PIXELVAL_AUTO);

        /* aWidth[i] = MAX(aExplicitWidth[i], aMinWidth[i]); */
        if (aExplicitWidth[i] == PIXELVAL_AUTO) {
            aWidth[i] = aMinWidth[i];
        } else {
            aWidth[i] = aExplicitWidth[i];
        }
        explicit += aWidth[i];
    }

    /* If the number of pixels already allocated is less than than the
     * available width, then distribute remaining pixels between the
     * columns with percentage or "auto" widths. Otherwise, do nothing,
     * layout is complete.
     *
     * Exactly how those pixels are distributed is the tricky bit:
     */
    if (explicit < availablewidth) {
        float percent = 0.0;       /* Total percent requested by % cols */
        float min_ratio = 0.0;     /* min pixels per percentage point */
        int other_pix = 0;         /* Number of pixels allocate to non % cols */
        int allocated;

	/* 1. Add pixels to columns with percentage widths to try to
         *    meet percentage width constraints.
         */
        for (i = 0; i < nCol; i++) {
            if (aExplicitWidth[i] == PIXELVAL_AUTO && aPercentWidth[i] >= 0.0) {
                if (aPercentWidth[i] > 0.01) {
                    float ratio = (float)aMinWidth[i] / aPercentWidth[i];
                    min_ratio = MAX(min_ratio, ratio);
                }
                percent += aPercentWidth[i];
            } else {
                other_pix += aWidth[i];
            }
        }
        if (percent < 100.0) {
            float other_percent = 100.0 - percent;
            float ratio = (float)other_pix / other_percent;
            min_ratio = MAX(min_ratio, ratio);
        }
        allocated = other_pix;
        for (i = 0; i < nCol; i++) {
            if (aExplicitWidth[i] == PIXELVAL_AUTO && aPercentWidth[i] >= 0.0) {
                int w = (int)(min_ratio * aPercentWidth[i]);
                assert(w >= (aMinWidth[i]-1));
                aWidth[i] = MAX(aMinWidth[i], w);
                allocated += aWidth[i];
            }
        }

        LOG {
            HtmlTree *pTree = pLayout->pTree;
            Tcl_Obj *pObj = Tcl_NewObj();
            Tcl_IncrRefCount(pObj);
            for (i = 0; i < nCol; i++) {
                Tcl_ListObjAppendElement(0, pObj, Tcl_NewIntObj(aWidth[i]));
            }
            HtmlLog(pTree, "LAYOUTENGINE", "%s tableCalculateCellWidths() %s",
                Tcl_GetString(HtmlNodeCommand(pTree, pData->pNode)), 
                Tcl_GetString(pObj)
            );
        }

        if (allocated < availablewidth && percent < 100.0 && other_pix > 0) {
	    /* In this case we grow any columns that had the 'width'
             * property set to "auto". All "auto" columns grow, in
             * proportion to the different between their max and min 
             * widths. Percentage width columns also grow here.
             */
            float other_percent = 100.0 - percent;
            float xtra_percent = 0.0;
            int requested = 0;   /* Total requested pixels by "auto" columns */
            int granted;
            int total_requested;

            for (i = 0; i < nCol; i++) {
                if (
                    aExplicitWidth[i] == PIXELVAL_AUTO && 
                    aPercentWidth[i] < 0.0
                ) {
                    requested += aMaxWidth[i] - aMinWidth[i];
                }
            }

            total_requested = (int)((float)(requested * 100) / other_percent);
            if (isAuto) {
                granted = MIN(availablewidth - allocated, total_requested);
            } else {
                granted = (availablewidth - allocated);
                if (percent < 0.01 && requested == 0) {
                    xtra_percent = (100.0 / (float)nCol);
                }
            }
            assert(total_requested >= 0);
            assert(granted >= 0);
            assert(granted > 0 || total_requested == 0);

            for (i = 0; i < nCol; i++) {
                if (aExplicitWidth[i] == PIXELVAL_AUTO) {
                    if (aPercentWidth[i] < 0.0) {
                        if (granted == total_requested || !total_requested) {
                            assert(aWidth[i] == aMinWidth[i]);
                            assert(aMaxWidth[i] >= aWidth[i]);
                            aWidth[i] = aMaxWidth[i];
                        } else {
                            int r = aMaxWidth[i] - aMinWidth[i];
                            r = r * ((float)granted / (float)total_requested);
                            assert(r >= 0);
                            aWidth[i] += r;
                        }
                        aWidth[i] += ((xtra_percent * (float)granted)/100.0);
                    } else {
                        int r =  (int)((aPercentWidth[i]*(float)granted)/100.0);
                        assert(r >= 0);
                        aWidth[i] += r;
                    }
                }
            }
        } 

        allocated = 0;
        for (i = 0; i < nCol; i++) {
            allocated += aWidth[i];
        }

        if (allocated < availablewidth && percent > 0.0) {
            int granted = 0;
            if (isAuto) {
                for (i = 0; i < nCol; i++) {
                    if (
                        aExplicitWidth[i] == PIXELVAL_AUTO &&
                        aPercentWidth[i] > 0.0 &&
                        aWidth[i] < aMaxWidth[i]
                    ) {
                        float d = (float)(aMaxWidth[i] - aWidth[i]);
                        int g = (int)(d * percent/ aPercentWidth[i]);
                        granted = MAX(granted, g);
                    }
                }
                granted = MIN(granted, availablewidth - allocated);
            } else {
                granted = availablewidth - allocated;
            }

            for (i = 0; i < nCol; i++) {
                if (aExplicitWidth[i]==PIXELVAL_AUTO && aPercentWidth[i]>0.0) {
                    int d = (int)((float)granted * (aPercentWidth[i]/percent));
                    aWidth[i] += d;
                    allocated += d;
                }
            }
        }

        if (availablewidth < allocated) {
            int freeable = 0;
            int nFree = 0;
            for (i = 0; i < nCol; i++) {
                if (
                    aExplicitWidth[i] == PIXELVAL_AUTO &&
                    aPercentWidth[i] > 0.0
                ) {
                    assert(aWidth[i] >= aMinWidth[i]);
                    freeable += (aWidth[i] - aMinWidth[i]);
                }
            }

            nFree = MIN(allocated - availablewidth, freeable);
            for (i = 0; i < nCol; i++) {
                if (
                    aExplicitWidth[i] == PIXELVAL_AUTO &&
                    aPercentWidth[i] > 0.0
                ) {
                    int t = (aWidth[i] - aMinWidth[i]);
                    int f = (int)((float)t * ((float)nFree / (float)freeable));
                    aWidth[i] -= f;
                }
            }
        }
    }
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
 *     The table layout algorithm used is described in section 17.5.2.2 of 
 *     the CSS 2.1 spec.
 *
 *     When this function is called, pBox->iContaining contains the width
 *     available to the table content - not including any margin, border or
 *     padding on the table itself. Any pixels allocated between the edge of
 *     the table and the leftmost or rightmost cell due to 'border-spacing' is
 *     included in pBox->iContaining. If the table element has a computed value
 *     for width other than 'auto', then pBox->iContaining is the calculated
 *     'width' value. Otherwise it is the width available according to the
 *     width of the containing block.
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
    HtmlNode *pNode;          /* The node to layout */
{
    HtmlTree *pTree = pLayout->pTree;
    HtmlComputedValues *pV = pNode->pPropertyValues;
    int nCol = 0;             /* Number of columns in this table */
    int i;
    int width;                /* Actual width of entire table */
    int availwidth;           /* Total width available for cells */

    int isAuto;               /* If the width property of <table> is "auto" */

    int *aMinWidth = 0;       /* Minimum width for each column */
    int *aMaxWidth = 0;       /* Minimum width for each column */
    float *aPercentWidth = 0; /* Percentage widths for each column */
    int *aWidth = 0;          /* Actual width for each column */
    int *aExplicitWidth = 0;  /* Actual width for each column */
    int *aY = 0;              /* Top y-coord for each row */
    TableCell *aCell = 0;     /* Array of nCol cells used during drawing */
    TableData data;

    memset(&data, 0, sizeof(struct TableData));
    data.pLayout = pLayout;
    data.pNode = pNode;

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
    tableIterate(pTree, pNode, tableCountCells, 0, &data);
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
    aMinWidth      = (int *)HtmlClearAlloc(nCol*sizeof(int));
    aMaxWidth      = (int *)HtmlClearAlloc(nCol*sizeof(int));
    aPercentWidth  = (float *)HtmlClearAlloc(nCol*sizeof(float));
    aExplicitWidth = (int *)HtmlClearAlloc(nCol*sizeof(int));
    aWidth         = (int *)HtmlClearAlloc(nCol*sizeof(int));

    aY = (int *)HtmlClearAlloc((data.nRow+1)*sizeof(int));
    aCell = (TableCell *)HtmlClearAlloc(data.nCol*sizeof(TableCell));

    data.aMaxWidth = aMaxWidth;
    data.aMinWidth = aMinWidth;
    data.aPercentWidth = aPercentWidth;
    data.aWidth = aWidth;
    data.aExplicitWidth = aExplicitWidth;

    /* Both aExplicitWidth and aPercentWidth are initialised to arrays of
     * value PIXELVAL_AUTO. aMaxWidth[] and aMinWidth[] are initially
     * zeroed. aWidth[] is also zeroed, but will be completely overwritten
     * by tableCalculateCellWidths() later.
     */
    for (i = 0; i < nCol; i++) {
        aExplicitWidth[i] = PIXELVAL_AUTO;
        aPercentWidth[i] = -1.0;
    }

    /* Calculate the minimum, maximum, and requested percentage widths of
     * each column.  The first pass only considers cells that span a single
     * column.  In this case the min/max width of each column is the maximum of
     * the min/max widths for all cells in the column.
     * 
     * If the table contains one or more cells that span more than one
     * column, we make a second pass. The min/max widths are increased,
     * if necessary, to account for the multi-column cell. In this case,
     * the width of each column that the cell spans is increased by 
     * the same amount (plus or minus a pixel to account for integer
     * rounding).
     */
    tableIterate(pTree, pNode, tableColWidthSingleSpan, 0, &data);
    tableIterate(pTree, pNode, tableColWidthMultiSpan, 0, &data);

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Interp *interp = pTree->interp;
        Tcl_Obj *pWidths = Tcl_NewObj();
        int ii;

        char zExplicit[24];
        char zPercent[24];

        for (ii = 0; ii < data.nCol; ii++) {
            if (aExplicitWidth[ii] != PIXELVAL_AUTO) {
                sprintf(zExplicit, "%d", aExplicitWidth[ii]);
            } else {
                sprintf(zExplicit, "auto");
            }

            if (aPercentWidth[ii] >= 0.0) {
                sprintf(zPercent, "%.2f%%", aPercentWidth[ii]);
            } else {
                sprintf(zPercent, "N/A");
            }

            HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout() "
                "Column %d: min=%d max=%d explicit=%s percent=%s",
                Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
                ii, data.aMinWidth[ii], aMaxWidth[ii], zExplicit, zPercent
            );
        }
    }

    /* Decide on some actual widths for the cells */
    isAuto = (pNode->pPropertyValues->iWidth == PIXELVAL_AUTO);
    if (pLayout->minmaxTest && pNode->pPropertyValues->mask & PROP_MASK_WIDTH) {
        isAuto = 1;
    }
    availwidth = (pBox->iContaining - (nCol+1) * data.border_spacing);
    tableCalculateCellWidths(&data, availwidth, isAuto);

    /* Get the actual table width based on the cell widths */
    width = (nCol+1) * data.border_spacing;
    for (i = 0; i < nCol; i++) {
        width += aWidth[i];
    }

    LOG {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Interp *interp = pTree->interp;
        Tcl_Obj *pWidths = Tcl_NewObj();
        int ii;
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout() "
            "Available table width = %d (%s)",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            availwidth, (isAuto ? "auto": "not auto")
        );

        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout() "
            "Actual table width = %d",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            width
        );

        /* Log the actual cell widths */
        pWidths = Tcl_NewObj();
        Tcl_IncrRefCount(pWidths);
        for (ii = 0; ii < data.nCol; ii++) {
            Tcl_Obj *pInt = Tcl_NewIntObj(data.aWidth[ii]);
            Tcl_ListObjAppendElement(interp, pWidths, pInt);
        }
        HtmlLog(pTree, "LAYOUTENGINE", "%s HtmlTableLayout() "
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
    tableIterate(pTree, pNode, tableDrawCells, tableDrawRow, &data);

    pBox->height = MAX(pBox->height, data.aY[data.nRow]);
    pBox->width = MAX(pBox->width, width);

    assert(pBox->height < 10000000);
    assert(pBox->width < 10000000);

    HtmlFree((char *)aMinWidth);
    HtmlFree((char *)aMaxWidth);
    HtmlFree((char *)aWidth);
    HtmlFree((char *)aY);
    HtmlFree((char *)aCell);
    HtmlFree((char *)aPercentWidth);

    return TCL_OK;
}

