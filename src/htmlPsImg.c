/*
 * So blatently ripped off from Tk, comments aren't even updated.
 * Peter MacDonald.  http://browsex.com
 *
 *      This module provides Postscript output support for canvases,
 *      including the "postscript" widget command plus a few utility
 *      procedures used for generating Postscript.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: htmlPsImg.c,v 1.1 2001/06/17 22:40:05 peter Exp $
 */

#ifndef _TCLHTML_

int
TkPostscriptImage(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tk_PostscriptInfo psInfo,	/* postscript info */
    XImage *ximage,		/* Image to draw */
    int x, int y,		/* First pixel to output */
    int width, int height)	/* Width and height of area */
{
    TkPostscriptInfo *psInfoPtr = (TkPostscriptInfo *) psInfo;
    char buffer[256];
    int xx, yy, band, maxRows;
    double red, green, blue;
    int bytesPerLine=0, maxWidth=0;
    int level = psInfoPtr->colorLevel;
    Colormap cmap;
    int i, depth, ncolors;
    Visual *visual;
    TkColormapData cdata;

    if (psInfoPtr->prepass) {
	return TCL_OK;
    }

    cmap = Tk_Colormap(tkwin);
    depth = Tk_Depth(tkwin);
    visual = Tk_Visual(tkwin);

    /*
     * Obtain information about the colormap, ie the mapping between
     * pixel values and RGB values.  The code below should work
     * for all Visual types.
     */

    ncolors = visual->map_entries;
    cdata.colors = (XColor *) ckalloc(sizeof(XColor) * ncolors);
    cdata.ncolors = ncolors;

    if (visual->class == DirectColor || visual->class == TrueColor) {
	cdata.separated = 1;
	cdata.red_mask = visual->red_mask;
	cdata.green_mask = visual->green_mask;
	cdata.blue_mask = visual->blue_mask;
	cdata.red_shift = 0;
	cdata.green_shift = 0;
	cdata.blue_shift = 0;
	while ((0x0001 & (cdata.red_mask >> cdata.red_shift)) == 0)
	    cdata.red_shift ++;
	while ((0x0001 & (cdata.green_mask >> cdata.green_shift)) == 0)
	    cdata.green_shift ++;
	while ((0x0001 & (cdata.blue_mask >> cdata.blue_shift)) == 0)
	    cdata.blue_shift ++;
	for (i = 0; i < ncolors; i ++)
	    cdata.colors[i].pixel =
		((i << cdata.red_shift) & cdata.red_mask) |
		((i << cdata.green_shift) & cdata.green_mask) |
		((i << cdata.blue_shift) & cdata.blue_mask);
    } else {
	cdata.separated=0;
	for (i = 0; i < ncolors; i ++)
	    cdata.colors[i].pixel = i;
    }
    if (visual->class == StaticGray || visual->class == GrayScale)
	cdata.color = 0;
    else
	cdata.color = 1;


    XQueryColors(Tk_Display(tkwin), cmap, cdata.colors, ncolors);

    /*
     * Figure out which color level to use (possibly lower than the 
     * one specified by the user).  For example, if the user specifies
     * color with monochrome screen, use gray or monochrome mode instead. 
     */

    if (!cdata.color && level == 2) {
	level = 1;
    }

    if (!cdata.color && cdata.ncolors == 2) {
	level = 0;
    }

    /*
     * Check that at least one row of the image can be represented
     * with a string less than 64 KB long (this is a limit in the 
     * Postscript interpreter).
     */

    switch (level)
    {
	case 0: bytesPerLine = (width + 7) / 8;  maxWidth = 240000;  break;
	case 1: bytesPerLine = width;  maxWidth = 60000;  break;
	case 2: bytesPerLine = 3 * width;  maxWidth = 20000;  break;
    }

    if (bytesPerLine > 60000) {
	Tcl_ResetResult(interp);
	sprintf(buffer,
		"Can't generate Postscript for images more than %d pixels wide",
		maxWidth);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	ckfree((char *) cdata.colors);
	return TCL_ERROR;
    }

    maxRows = 60000 / bytesPerLine;

    for (band = height-1; band >= 0; band -= maxRows) {
	int rows = (band >= maxRows) ? maxRows : band + 1;
	int lineLen = 0;
	switch (level) {
	    case 0:
		sprintf(buffer, "%d %d 1 matrix {\n<", width, rows);
		Tcl_AppendResult(interp, buffer, (char *) NULL);
		break;
	    case 1:
		sprintf(buffer, "%d %d 8 matrix {\n<", width, rows);
		Tcl_AppendResult(interp, buffer, (char *) NULL);
		break;
	    case 2:
		sprintf(buffer, "%d %d 8 matrix {\n<",
			width, rows);
		Tcl_AppendResult(interp, buffer, (char *) NULL);
		break;
	}
	for (yy = band; yy > band - rows; yy--) {
	    switch (level) {
		case 0: {
		    /*
		     * Generate data for image in monochrome mode.
		     * No attempt at dithering is made--instead, just
		     * set a threshold.
		     */
		    unsigned char mask=0x80;
		    unsigned char data=0x00;
		    for (xx = x; xx< x+width; xx++) {
			TkImageGetColor(&cdata, XGetPixel(ximage, xx, yy),
					&red, &green, &blue);
			if (0.30 * red + 0.59 * green + 0.11 * blue > 0.5)
			    data |= mask;
			mask >>= 1;
			if (mask == 0) {
			    sprintf(buffer, "%02X", data);
			    Tcl_AppendResult(interp, buffer, (char *) NULL);
			    lineLen += 2;
			    if (lineLen > 60) {
			        lineLen = 0;
			        Tcl_AppendResult(interp, "\n", (char *) NULL);
			    }
			    mask=0x80;
			    data=0x00;
			}
		    }
		    if ((width % 8) != 0) {
		        sprintf(buffer, "%02X", data);
		        Tcl_AppendResult(interp, buffer, (char *) NULL);
		        mask=0x80;
		        data=0x00;
		    }
		    break;
		}
		case 1: {
		    /*
		     * Generate data in gray mode--in this case, take a 
		     * weighted sum of the red, green, and blue values.
		     */
		    for (xx = x; xx < x+width; xx ++) {
			TkImageGetColor(&cdata, XGetPixel(ximage, xx, yy),
					&red, &green, &blue);
			sprintf(buffer, "%02X", (int) floor(0.5 + 255.0 *
							    (0.30 * red +
							     0.59 * green +
							     0.11 * blue)));
			Tcl_AppendResult(interp, buffer, (char *) NULL);
			lineLen += 2;
			if (lineLen > 60) {
			    lineLen = 0;
			    Tcl_AppendResult(interp, "\n", (char *) NULL);
			}
		    }
		    break;
		}
		case 2: {
		    /*
		     * Finally, color mode.  Here, just output the red, green,
		     * and blue values directly.
		     */
			for (xx = x; xx < x+width; xx++) {
			TkImageGetColor(&cdata, XGetPixel(ximage, xx, yy),
				&red, &green, &blue);
			sprintf(buffer, "%02X%02X%02X",
				(int) floor(0.5 + 255.0 * red),
				(int) floor(0.5 + 255.0 * green),
				(int) floor(0.5 + 255.0 * blue));
			Tcl_AppendResult(interp, buffer, (char *) NULL);
			lineLen += 6;
			if (lineLen > 60) {
			    lineLen = 0;
			    Tcl_AppendResult(interp, "\n", (char *) NULL);
			}
		    }
		    break;
		}
	    }
	}
	switch (level) {
	    case 0: sprintf(buffer, ">\n} image\n"); break;
	    case 1: sprintf(buffer, ">\n} image\n"); break;
	    case 2: sprintf(buffer, ">\n} false 3 colorimage\n"); break;
	}
	Tcl_AppendResult(interp, buffer, (char *) NULL);
	sprintf(buffer, "0 %d translate\n", rows);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
    }
    ckfree((char *) cdata.colors);
    return TCL_OK;
}
int
Tk_PostscriptImage(
    Tk_Image image,		/* Token for image to redisplay. */
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tk_PostscriptInfo psinfo,	/* postscript info */
    int x, int y,		/* Upper-left pixel of region in image that
				 * needs to be redisplayed. */
    int width, int height,	/* Dimensions of region to redraw. */
    int prepass)
{
    int result;
    XImage *ximage;
    Pixmap pmap;
    GC newGC;
    XGCValues gcValues;
    if (prepass) {
	return TCL_OK;
    }

    /*
     * Create a Pixmap, tell the image to redraw itself there, and then
     * generate an XImage from the Pixmap.  We can then read pixel 
     * values out of the XImage.
     */

    pmap = Tk_GetPixmap(Tk_Display(tkwin), Tk_WindowId(tkwin),
                        width, height, Tk_Depth(tkwin));

    gcValues.foreground = WhitePixelOfScreen(Tk_Screen(tkwin));
    newGC = Tk_GetGC(tkwin, GCForeground, &gcValues);
    if (newGC != None) {
	XFillRectangle(Tk_Display(tkwin), pmap, newGC,
		0, 0, (unsigned int)width, (unsigned int)height);
	Tk_FreeGC(Tk_Display(tkwin), newGC);
    }

    Tk_RedrawImage(image, x, y, width, height, pmap, 0, 0);

    ximage = XGetImage(Tk_Display(tkwin), pmap, 0, 0,
	    (unsigned int)width, (unsigned int)height, AllPlanes, ZPixmap);

    Tk_FreePixmap(Tk_Display(tkwin), pmap);
    
    if (ximage == NULL) {
	/* The XGetImage() function is apparently not
	 * implemented on this system. Just ignore it.
	 */
	return TCL_OK;
    }
    result = TkPostscriptImage(interp, tkwin, psinfo, ximage, x, y,
	    width, height);

    XDestroyImage(ximage);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RedrawImage --
 *
 *	This procedure is called by widgets that contain images in order
 *	to redisplay an image on the screen or an off-screen pixmap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image's manager is notified, and it redraws the desired
 *	portion of the image before returning.
 *
 *----------------------------------------------------------------------
 */

void
Tk_RedrawImage(
    Tk_Image image,		/* Token for image to redisplay. */
    int imageX, int imageY,	/* Upper-left pixel of region in image that
				 * needs to be redisplayed. */
    int width, int height,	/* Dimensions of region to redraw. */
    Drawable drawable,		/* Drawable in which to display image
				 * (window or pixmap).  If this is a pixmap,
				 * it must have the same depth as the window
				 * used in the Tk_GetImage call for the
				 * image. */
    int drawableX, int drawableY)/* Coordinates in drawable that correspond
				 * to imageX and imageY. */
{
    Image *imagePtr = (Image *) image;

    if (imagePtr->masterPtr->typePtr == NULL) {
	/*
	 * No master for image, so nothing to display.
	 */

	return;
    }

    /*
     * Clip the redraw area to the area of the image.
     */

    if (imageX < 0) {
	width += imageX;
	drawableX -= imageX;
	imageX = 0;
    }
    if (imageY < 0) {
	height += imageY;
	drawableY -= imageY;
	imageY = 0;
    }
    if ((imageX + width) > imagePtr->masterPtr->width) {
	width = imagePtr->masterPtr->width - imageX;
    }
    if ((imageY + height) > imagePtr->masterPtr->height) {
	height = imagePtr->masterPtr->height - imageY;
    }
    (*imagePtr->masterPtr->typePtr->displayProc)(
	    imagePtr->instanceData, imagePtr->display, drawable,
	    imageX, imageY, width, height, drawableX, drawableY);
}

void
Tk_SizeOfImage(
    Tk_Image image,		/* Token for image whose size is wanted. */
    int *widthPtr,		/* Return width of image here. */
    int *heightPtr)		/* Return height of image here. */
{
    Image *imagePtr = (Image *) image;

    *widthPtr = imagePtr->masterPtr->width;
    *heightPtr = imagePtr->masterPtr->height;
}

void
Tk_DeleteImage(
    Tcl_Interp *interp,		/* Interpreter in which the image was
				 * created. */
    char *name)			/* Name of image. */
{
    Tcl_HashEntry *hPtr;
    TkWindow *winPtr;

    winPtr = (TkWindow *) Tk_MainWindow(interp);
    if (winPtr == NULL) {
	return;
    }
    hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, name);
    if (hPtr == NULL) {
	return;
    }
    DeleteImage((ImageMaster *) Tcl_GetHashValue(hPtr));
}

static void
DeleteImage( ImageMaster *masterPtr)	/* Pointer to main data structure for image. */
{
    Image *imagePtr;
    Tk_ImageType *typePtr;

    typePtr = masterPtr->typePtr;
    masterPtr->typePtr = NULL;
    if (typePtr != NULL) {
	for (imagePtr = masterPtr->instancePtr; imagePtr != NULL;
		imagePtr = imagePtr->nextPtr) {
	   (*typePtr->freeProc)(imagePtr->instanceData,
		   imagePtr->display);
	   (*imagePtr->changeProc)(imagePtr->widgetClientData, 0, 0,
		    masterPtr->width, masterPtr->height, masterPtr->width,
		    masterPtr->height);
	}
	(*typePtr->deleteProc)(masterPtr->masterData);
    }
    if (masterPtr->instancePtr == NULL) {
	Tcl_DeleteHashEntry(masterPtr->hPtr);
	ckfree((char *) masterPtr);
    }
}

void
TkDeleteAllImages(
    TkMainInfo *mainPtr)	/* Structure describing application that is
				 * going away. */
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    ImageMaster *masterPtr;

    for (hPtr = Tcl_FirstHashEntry(&mainPtr->imageTable, &search);
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
	DeleteImage(masterPtr);
    }
    Tcl_DeleteHashTable(&mainPtr->imageTable);
}

ClientData
Tk_GetImageMasterData(
    Tcl_Interp *interp,		/* Interpreter in which the image was
				 * created. */
    char *name,			/* Name of image. */
    Tk_ImageType **typePtrPtr)	/* Points to location to fill in with
				 * pointer to type information for image. */
{
    Tcl_HashEntry *hPtr;
    TkWindow *winPtr;
    ImageMaster *masterPtr;

    winPtr = (TkWindow *) Tk_MainWindow(interp);
    hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->imageTable, name);
    if (hPtr == NULL) {
	*typePtrPtr = NULL;
	return NULL;
    }
    masterPtr = (ImageMaster *) Tcl_GetHashValue(hPtr);
    *typePtrPtr = masterPtr->typePtr;
    return masterPtr->masterData;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetTSOrigin --
 *
 *	Set the pattern origin of the tile to a common point (i.e. the
 *	origin (0,0) of the top level window) so that tiles from two
 *	different widgets will match up.  This done by setting the
 *	GCTileStipOrigin field is set to the translated origin of the
 *	toplevel window in the hierarchy.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The GCTileStipOrigin is reset in the GC.  This will cause the
 *	tile origin to change when the GC is used for drawing.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
Tk_SetTSOrigin(
    Tk_Window tkwin,
    GC gc,
    int x, int y)
{
    while (!Tk_IsTopLevel(tkwin)) {
	x -= Tk_X(tkwin) + Tk_Changes(tkwin)->border_width;
	y -= Tk_Y(tkwin) + Tk_Changes(tkwin)->border_width;
	tkwin = Tk_Parent(tkwin);
    }
    XSetTSOrigin(Tk_Display(tkwin), gc, x, y);
}

#endif /* _TCLHTML_ */
