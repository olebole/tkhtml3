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
static const char rcsid[] = "$Id: htmltable.c,v 1.81 2006/06/04 12:53:48 danielk1977 Exp $";

#include "htmllayout.h"

#define LOG if (pLayout->pTree->options.logcmd && !pLayout->minmaxTest)

/* Roughly convert a double value to an integer. */
#define INTEGER(x) ((int)((x) + ((x > 0.0) ? 0.49 : -0.49)))

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

        int currentmin;
        int currentmax;
        int minincr;
        int maxincr;

        int nAutoPixels = 0;      /* Aggregate (max - min) for auto columns */
        int nAutoColumns = 0;     /* Number of auto columns */

        /* For now, only increase the minimum and maximum widths. Presumably
         * the computed value of the 'width' property should be used to 
         * modify TableData.aExplicitWidth and TableData.aPercentWidth, but
         * it's not yet clear exactly how.
         * 
         * Also define a macro to test if a column has "width:auto". Note
         * that this macro is only valid inside this scope. It is explicitly 
         * undefined at the end of this {} block.
         */
        int *aMinWidth      = pData->aMinWidth;
        int *aMaxWidth      = pData->aMaxWidth;
        int *aExplicitWidth = pData->aExplicitWidth;
        int *aPercentWidth  = pData->aPercentWidth;
 
        /* Macro evaluates to true for an "auto-width" column. */
        #define COL_ISAUTO(i) \
             (aExplicitWidth[i]==PIXELVAL_AUTO && aPercentWidth[i]<0.0)

	/* Calculate the current collective minimum and maximum widths of the
	 * spanned columns. Also accumulate the aggregate difference between
	 * the maximum and minimum widths of any columns with "width:auto".
         */
	currentmin = (pData->border_spacing * (colspan-1));
        for (i=col; i<(col+colspan); i++) {
            currentmin += aMinWidth[i];
            if (COL_ISAUTO(i)) {
                nAutoPixels += aMaxWidth[i] - aMinWidth[i];
                nAutoColumns++;
            }
        }

        /* Calculate the maximum and minimum widths of this cell, including
         * border and padding (table-cells do not have margins). 
         */
        blockMinMaxWidth(pData->pLayout, pNode, &min, &max);
        nodeGetBoxProperties(pData->pLayout, pNode, 0, &box);
        min += box.iLeft + box.iRight;
        max += box.iLeft + box.iRight;

	/* Set minincr to the number of pixels that must be added to the
         * minimum widths of the spanned columns.
         *
	 * We need to somehow add these pixels to the minimum widths of the
	 * spanned columns. How's this for an approach:
         *
         * The "minincr" pixels are then assigned to columns as follows:
         *
	 *     1. If there are columns with no explicit or percentage width,
	 *        then distribute the minincr pixels between them in proportion
	 *        to (max - min). If any column has (min > max), then set 
         *        max = min. 
         *
         *     2. TODO.
         */
        minincr = MAX(0, min - currentmin);

        if (nAutoColumns > 0 && minincr > 0) {
            int nAutoColumnsRemaining = nAutoColumns;
            float ratio = (float)minincr / (float)nAutoPixels;
            for (i=col; i<(col+colspan); i++) {
                assert(nAutoColumnsRemaining >= 0);
                if (COL_ISAUTO(i)) {
                    if (nAutoColumnsRemaining > 1) {
                        int add; 
                        if (nAutoPixels > 0) {
                            add = INTEGER(ratio * (aMaxWidth[i]-aMinWidth[i]));
                        } else {
                            add = minincr / nAutoColumns;
                        }
                        minincr -= add;
                        aMinWidth[i] += add;
                    } else if (minincr > 0) {
                        assert(nAutoColumnsRemaining == 1);
                        aMinWidth[i] += minincr;
                        minincr = 0;
                    }
                    nAutoColumnsRemaining--;
                }
            }
        }

        /* If we failed to allocate all the min-width pixels to auto columns
         * in the loop above, distribute them evenly between columns here.
         */
        for (i=col; i<(col+colspan); i++) {
            int this_incr = (minincr / (col + colspan - i));
            minincr -= this_incr;
            aMinWidth[i] += this_incr;
            aMaxWidth[i] = MAX(aMinWidth[i], aMaxWidth[i]);
        }

	/* Divide any max-width pixels between all spanned columns. */
        for (i=col; i<(col+colspan); i++) {
            aMaxWidth[i] = MAX(max / colspan, aMaxWidth[i]);
        }

        #undef COL_ISAUTO
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
                p->aRowSpan = (int *)HtmlRealloc(0, (char *)p->aRowSpan, 
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
    int eDisplay = DISPLAY(pNode->pPropertyValues);
    RowIterateContext *p = (RowIterateContext *)clientData;

    switch (eDisplay) {
        case CSS_CONST_TABLE_ROW: {
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
        case CSS_CONST_NONE: {
            return HTML_WALK_DO_NOT_DESCEND;
        }
        default: {
            /* If the node has a display type other than 'table-row' or 
             * 'none' then descend. We do this because people often put
             * <form> tags in the middle of table structures.
             */
            return HTML_WALK_DESCEND;
        }
    }

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
    HtmlFree(0, (char *)sRowContext.aRowSpan);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * allocatePixels --
 *
 *     This function is used by tableCalculateCellWidths() to divide
 *     a set number of pixels between columns.
 *
 * Results:
 *     Returns the number of pixels allocated (less than or equal to
 *     iAvailable).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
allocatePixels(iAvailable, nCol, aRequested, aWidth)
    int iAvailable;        /* Number of pixels available */
    int nCol;              /* Number of columns to distribute pixels between */
    int *aRequested;       /* Array of requested pixels (per column) */
    int *aWidth;           /* IN/OUT: Column widths */
{
    int iTotalRequest = 0;
    int iRet = iAvailable;
    int i;

    assert(iAvailable >= 0);
    assert(nCol >= 0);

    for (i = 0; i < nCol; i++) {
        assert(aWidth[i] >= 0);
        assert(aRequested[i] >= 0);
        iTotalRequest += aRequested[i];
    }

    if (iAvailable >= iTotalRequest) {
        for (i = 0; i < nCol; i++) {
            aWidth[i] += aRequested[i];
        }
        iRet = iTotalRequest;
    } else {
        int iRemaining = iAvailable;
        assert(iTotalRequest > 0);
        for (i = 0; i < nCol && iTotalRequest > 0; i++) {
            int alloc = (aRequested[i] * iRemaining) / iTotalRequest;
            aWidth[i] += alloc;
            iRemaining -= alloc;
            iTotalRequest -= aRequested[i];
        }
        assert(iRemaining == 0);
    }
    return iRet;
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
 *         * A percentage pixel width (if 'width' is a %).
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
 *            or calculated according to their assigned percentages with
 *            respect to the available width.
 *
 *     Using this algorithm the actual sum of the assigned table widths may
 *     be greater than or less than the value passed as the second
 *     parameter to this function. The actual column widths should be used
 *     to calculate an actual table width - irrespective of the 'width'
 *     attribute - after this function has run.
 *
 *     Note: "BasicTableLayoutStrategy.cpp" contains the analogous Gecko code.
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
    int iRem;                 /* Pixels remaining (from availablewidth) */
    int nCol = pData->nCol;
    LayoutContext *pLayout = pData->pLayout;

    /* Local handles for the input arrays */
    int   *aWidth         = pData->aWidth;
    int   *aMinWidth      = pData->aMinWidth;
    int   *aMaxWidth      = pData->aMaxWidth;
    float *aPercentWidth  = pData->aPercentWidth;
    int   *aExplicitWidth = pData->aExplicitWidth;

#define COL_ISPERCENT(x)  (aPercentWidth[x] >= 0.01)
#define COL_ISEXPLICIT(x) (aExplicitWidth[x] != PIXELVAL_AUTO)
#define COL_ISAUTO(x)     (!COL_ISEXPLICIT(x) && !COL_ISPERCENT(x))

    /* Summary of columns with percentage widths */
    float min_ratio;      /* Minimum desired pixels per percentage point */
    float exp_ratio;      /* Minimum desired pixels per percentage point */
    float percent_sum;    /* Sum of percentage widths */
    int isPercentOver;    /* True if (percent_sum >= 100.0) */

    int nPercentWidth;    /* Number of columns with percentage widths */
    int nAutoWidth;       /* Number of columns with "auto" widths */

    int *aRequested;      /* Array used as arg to allocatePixels() */

    /* The following two variables are used only within "LOG {...}" blocks.
     * They are used to accumlate information that is logged before
     * this function returns. 
     */
    static const int NUM_LOGVALUES = 5; 
    int  *aLogValues = 0;
    int STEP = 0;
    LOG { 
        int nBytes = sizeof(int) * nCol * (NUM_LOGVALUES * 2);
        aLogValues = (int *)HtmlAlloc(0, nBytes);
    }

    /* A rather lengthy block to log the inputs to this function. */
    LOG {
        HtmlTree *pTree = pLayout->pTree;
        int ii;

        char zPercent[24];
        Tcl_Obj *pLog = Tcl_NewObj();
        Tcl_IncrRefCount(pLog);

        Tcl_AppendToObj(pLog, "Inputs to column width algorithm: ", -1);
        Tcl_AppendToObj(pLog, "<p>Available width is ", -1);
        Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(availablewidth));
        Tcl_AppendToObj(pLog, "  (width property was <b>", -1);
        Tcl_AppendToObj(pLog, isAuto ? "auto</b>" : "not</b> auto", -1);
        Tcl_AppendToObj(pLog, ")</p>", -1);

        Tcl_AppendToObj(pLog, 
            "<table><tr>"
            "  <th>Col Number"
            "  <th>Min Content Width"
            "  <th>Max Content Width"
            "  <th>Explicit Width"
            "  <th>Percentage Width", -1);

        for (ii = 0; ii < nCol; ii++) {
            int jj;

            Tcl_AppendToObj(pLog, "<tr><td>", -1);
            Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(ii));

            for (jj = 0; jj < 3; jj++) {
                int val;
                switch (jj) {
                    case 0: val = aMinWidth[ii]; break;
                    case 1: val = aMaxWidth[ii]; break;
                    case 2: val = aExplicitWidth[ii]; break;
                }
                Tcl_AppendToObj(pLog, "<td>", -1);
                if (val != PIXELVAL_AUTO) {
                    Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(val));
                    Tcl_AppendToObj(pLog, "px", -1);
                } else {
                    Tcl_AppendToObj(pLog, "N/A", -1);
                }
            }

            Tcl_AppendToObj(pLog, "<td>", -1);
            if (aPercentWidth[ii] >= 0.0) {
                sprintf(zPercent, "%.2f%%", aPercentWidth[ii]);
            } else {
                sprintf(zPercent, "N/A");
            }
            Tcl_AppendToObj(pLog, zPercent, -1);
        }
        Tcl_AppendToObj(pLog, "</table>", -1);

        HtmlLog(pTree, "LAYOUTENGINE", "%s tableCalculateCellWidths() %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pData->pNode)), 
            Tcl_GetString(pLog)
        );

        Tcl_DecrRefCount(pLog);
    }

    /* Allocate the aRequested array. It will be freed before returning. */
    aRequested = (int *)HtmlAlloc(0, sizeof(int) * nCol);

    /* Step 1. Allocate each column it's minimum content width. */
    iRem = availablewidth;
    for (i = 0; i < nCol; i++) {
        assert(aWidth[i] == 0);
        assert(aExplicitWidth[i] >= 0 || aExplicitWidth[i] == PIXELVAL_AUTO);
        aWidth[i] = aMinWidth[i];
        iRem -= aWidth[i];
    }
    LOG { 
         memcpy(&aLogValues[STEP++ * nCol], aWidth, sizeof(int) * nCol); 
         memcpy(&aLogValues[STEP++ * nCol], aWidth, sizeof(int) * nCol); 
    }

    /* Variable iRem contains the number of remaining pixels to split up
     * between the columns. At this point, iRem may be negative, if the
     * minimum-content-width of the cells is less than the available width.
     * In this case, set iRem to zero so that subsequent code does not
     * have to deal with a negative value. 
     *
     * If the subsequent operations allocate more than iRem pixels (making 
     * iRem negative), this is a bug. They are not supposed to do that. Only
     * the "allocate minimum widths" step above can force a table to be wider
     * than availablewidth.
     */
    iRem = MAX(0, iRem);

    /* Analyse any columns with percentage widths. This block sets the
     * min_ratio, exp_ratio, percent_sum and nPercentWidth variables.
     */ 
    min_ratio = 0.0;    /* Minimum desired pixels per percentage point */
    exp_ratio = 0.0;    /* Explicitly desired pixels per percentage point */
    percent_sum = 0.0;
    nPercentWidth = 0;
    nAutoWidth = 0;
    for (i = 0; i < nCol; i++) {
        /* Check the integrity of the COL_xxx macros. A column may be
         * either "auto", or one or both of "percent" and "explicit". 
         */
        assert(COL_ISAUTO(i) || COL_ISPERCENT(i) || COL_ISEXPLICIT(i));
        assert(!(COL_ISAUTO(i) && COL_ISPERCENT(i)));
        assert(!(COL_ISAUTO(i) && COL_ISEXPLICIT(i)));

        if (COL_ISPERCENT(i)) {
            float m;
            percent_sum += aPercentWidth[i];
            nPercentWidth++;

            m = ((float)aWidth[i]) / aPercentWidth[i];
            min_ratio = MAX(min_ratio, m);

            if (aExplicitWidth >= 0) {
                m = ((float)aExplicitWidth[i]) / aPercentWidth[i];
                exp_ratio = MAX(exp_ratio, m);
            }
        } 
        if (COL_ISAUTO(i)) {
            nAutoWidth++;
        }
    }
    isPercentOver = ((percent_sum > 99.9) ? 1 : 0);

    if (!isPercentOver && nPercentWidth < nCol) {
        int iTotalOtherMinWidth = 0;
        int iTotalOtherExpWidth = 0;
        for (i = 0; i < nCol; i++) {
            if (COL_ISPERCENT(i)) {
                iTotalOtherMinWidth += aWidth[i];
                iTotalOtherExpWidth += MAX(0, aExplicitWidth[i]);
            }
        }
        min_ratio = MAX(min_ratio,
            (float)(iTotalOtherMinWidth) / (100.0 - percent_sum)
        );
        exp_ratio = MAX(exp_ratio,
            (float)(iTotalOtherExpWidth) / (100.0 - percent_sum)
        );
    } else if (percent_sum >= 0.01) {
        /* If the sum of the % widths is greater than 100.0, divide
         * up all the remaining space amongst percentage columns.
         */
        min_ratio = ((float)(availablewidth)) / percent_sum;
    }

    /* Step 2. Add pixels to columns with percentage widths to try to
     * satisfy percentage constraints. Do not exceed the available-width
     * in pursuit of this goal. 
     */
    memset(aRequested, 0, nCol * sizeof(int));
    if (nPercentWidth > 0) {
        /* Try to grow columns with % widths to meet the % constraints. */
        for (i = 0; i < nCol; i++) {
            if (aPercentWidth[i] >= 0.01) {
                int diff = ((min_ratio * aPercentWidth[i]) - aWidth[i]);
                aRequested[i] = MAX(diff, 0);
            }
        }
        iRem -= allocatePixels(iRem, nCol, aRequested, aWidth);
    }
    LOG { 
         memcpy(&aLogValues[STEP++ * nCol], aRequested, sizeof(int) * nCol); 
         memcpy(&aLogValues[STEP++ * nCol], aWidth, sizeof(int) * nCol); 
    }

    /* Step 3. Allocate extra pixels to columns with explicit widths. */
    for (i = 0; i < nCol; i++) {
        int desired = 0;
        if (COL_ISPERCENT(i)) {
            desired = (exp_ratio * aPercentWidth[i]);
        } else {
            desired = MAX(0, aExplicitWidth[i]);
        }
        aRequested[i] = MAX(0, desired - aWidth[i]);
    }
    iRem -= allocatePixels(iRem, nCol, aRequested, aWidth);
    LOG { 
        memcpy(&aLogValues[STEP++ * nCol], aRequested, sizeof(int) * nCol); 
        memcpy(&aLogValues[STEP++ * nCol], aWidth, sizeof(int) * nCol); 
    }
    
    /* Step 4. Allocate extra pixels to columns with "auto" widths. Do
     * not exceed the maximum content widths of any "auto" columns in this
     * step.
     */
    memset(aRequested, 0, nCol * sizeof(int));
    if (percent_sum < 99.9) {
        int iAutoRequest = 0;
        float max_ratio;

        for (i = 0; i < nCol; i++) {
            if (aPercentWidth[i] < 0.01 && aExplicitWidth[i] == PIXELVAL_AUTO) {
                int req = MAX(0, aMaxWidth[i] - aWidth[i]);
                aRequested[i] = req;
                iAutoRequest += req;
            }
        }
        max_ratio = (float)iAutoRequest / (100.0 - percent_sum);
        for (i = 0; i < nCol; i++) {
            if (aPercentWidth[i] >= 0.01) {
                aRequested[i] = (max_ratio * aPercentWidth[i]);
            }
        }

        iRem -= allocatePixels(iRem, nCol, aRequested, aWidth);
    }
    LOG { 
        memcpy(&aLogValues[STEP++ * nCol], aRequested, sizeof(int) * nCol); 
        memcpy(&aLogValues[STEP++ * nCol], aWidth, sizeof(int) * nCol); 
    }

    /* If the width of the table was specified as "auto", then we are
     * finished. Attempting to allocate any more space would force columns
     * to be wider than their maximum content widths. We only do this
     * if the width of the table was explicitly specified. In that case, 
     * proceed with:
     *
     * Step 5. 
     */
    if (!isAuto && nAutoWidth > 0 && !isPercentOver) {
        float ratio;
        float auto_ratio;

        int iTotalAutoWidth = 0;
        for (i = 0; i < nCol; i++) {
            if (COL_ISAUTO(i)) {
                iTotalAutoWidth += aWidth[i];
            }
        }

        if (iTotalAutoWidth == 0) {
            for (i = 0; i < nCol; i++) {
                if (COL_ISAUTO(i)) {
                    assert(aWidth[i] == 0);
                    iRem = MAX(0, iRem - 1);
                    aWidth[i] = 1;
                }
            }
            iTotalAutoWidth = nAutoWidth;
        }

        auto_ratio = ((float)(availablewidth * 2)/ (float)iTotalAutoWidth);
        ratio = ((float)(availablewidth * 2) / (100.0 - percent_sum));
        
        for (i = 0; i < nCol; i++) {
            if (aPercentWidth[i] >= 0.01) {
                aRequested[i] = MAX(0, ratio * aPercentWidth[i]);
            } else if (aExplicitWidth[i] == PIXELVAL_AUTO) {
                aRequested[i] = MAX(0, auto_ratio * aWidth[i]);
            } else {
                aRequested[i] = 0;
            }
        }
        iRem -= allocatePixels(iRem, nCol, aRequested, aWidth);
    }
    LOG { 
        memcpy(&aLogValues[STEP++ * nCol], aRequested, sizeof(int) * nCol); 
        memcpy(&aLogValues[STEP++ * nCol], aWidth, sizeof(int) * nCol); 
    }

    /* Log the outputs of this function. */
    LOG {
        int ii;
        int gg;
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Obj *pLog = Tcl_NewObj();
        Tcl_IncrRefCount(pLog);

        assert(STEP == (NUM_LOGVALUES * 2));

        Tcl_AppendToObj(pLog, "Results of column width algorithm.", -1);
        Tcl_AppendToObj(pLog, "<ol>"
            "<li>Allocate min content width to each column."
            "<li>Grow columns with % widths to meet % constraint."
            "<li>Allocate pixels to columns with explicit widths. Columns"
            "    with % widths grow here to, to match the constraints."
            "<li>Allocate pixels to columns with \"auto\" widths, not"
            "    exceeding their maximum content widths. Columns"
            "    with % widths grow also."
            "</ol>"
        , -1);
        Tcl_AppendToObj(pLog, "<table><tr><th>Col Number", -1);
        for (gg = 0; gg < NUM_LOGVALUES; gg++) {
            Tcl_AppendToObj(pLog, "<th>Req. ", -1);
            Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(gg + 1));
            Tcl_AppendToObj(pLog, "<th>Step ", -1);
            Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(gg + 1));
        }

        for (ii = 0; ii < nCol; ii++) {
            Tcl_AppendToObj(pLog, "<tr><td>", -1);
            Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(ii));

            for (gg = 0; gg < NUM_LOGVALUES * 2; gg++) {
                Tcl_AppendToObj(pLog, "<td>", -1);
                Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(aLogValues[ii+nCol*gg]));
                Tcl_AppendToObj(pLog, "px", -1);
            }
        }
        Tcl_AppendToObj(pLog, "<tr><td>Total", -1);
        for (gg = 0; gg < NUM_LOGVALUES * 2; gg++) {
            int iTotal = 0;
            for (ii = 0; ii < nCol; ii++) {
                iTotal += aLogValues[ii + nCol*gg];
            }
            Tcl_AppendToObj(pLog, "<td>", -1);
            Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(iTotal));
            Tcl_AppendToObj(pLog, "px", -1);
        }
        Tcl_AppendToObj(pLog, "</table>", -1);

        HtmlLog(pTree, "LAYOUTENGINE", "%s tableCalculateCellWidths() %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pData->pNode)), 
            Tcl_GetString(pLog)
        );
        Tcl_DecrRefCount(pLog);
    }

    LOG {
        HtmlFree(0, aLogValues);
    }
    HtmlFree(0, aRequested);
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
    aMinWidth      = (int *)HtmlClearAlloc(0, nCol*sizeof(int));
    aMaxWidth      = (int *)HtmlClearAlloc(0, nCol*sizeof(int));
    aPercentWidth  = (float *)HtmlClearAlloc(0, nCol*sizeof(float));
    aExplicitWidth = (int *)HtmlClearAlloc(0, nCol*sizeof(int));
    aWidth         = (int *)HtmlClearAlloc(0, nCol*sizeof(int));

    aY = (int *)HtmlClearAlloc(0, (data.nRow+1)*sizeof(int));
    aCell = (TableCell *)HtmlClearAlloc(0, data.nCol*sizeof(TableCell));

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
    
    /* Now actually draw the cells. */
    data.aY = aY;
    data.aCell = aCell;
    data.pBox = pBox;
    tableIterate(pTree, pNode, tableDrawCells, tableDrawRow, &data);

    pBox->height = MAX(pBox->height, data.aY[data.nRow]);
    pBox->width = MAX(pBox->width, width);

    assert(pBox->height < 10000000);
    assert(pBox->width < 10000000);

    HtmlFree(0, (char *)aMinWidth);
    HtmlFree(0, (char *)aMaxWidth);
    HtmlFree(0, (char *)aWidth);
    HtmlFree(0, (char *)aY);
    HtmlFree(0, (char *)aCell);
    HtmlFree(0, (char *)aPercentWidth);
    HtmlFree(0, (char *)aExplicitWidth);

    return TCL_OK;
}

