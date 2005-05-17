
/*
 * htmlimage.c ---
 *
 *     This file contains routines that manipulate images used in an HTML
 *     document.
 *
 * TODO: Copyright.
 */
#include "html.h"

/*
 *---------------------------------------------------------------------------
 *
 * imageChanged --
 *
 *     Dummy image-changed callback. Does nothing.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void imageChanged(clientData, x, y, width, height, imageWidth, imageHeight)
    ClientData clientData;
    int x;
    int y;
    int width;
    int height;
    int imageWidth;
    int imageHeight;
{
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlResizeImage --
 *
 *     This function manages getting and setting the size of images
 *     displayed by the html widget. 
 *
 *     Parameter zImage is the name of an image object created by some
 *     callback script for display by the html widget. pWidth and pHeight
 *     are pointers to integers that store the requested width and height
 *     of the image. If the intrinsic width or height is desired, then
 *     *pWidth or *pHeight should be set to -1.
 *
 *     The value returned is a Tcl_Obj* containing the name of an image
 *     scaled as requested.
 *
 * Results:
 *     Name of scaled image.
 *
 * Side effects:
 *     Modifies entries in HtmlTree.aImage.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj *
HtmlResizeImage(pTree, zImage, pWidth, pHeight)
    HtmlTree *pTree;
    CONST char *zImage;
    int *pWidth;
    int *pHeight;
{
    HtmlScaledImage *pImage;          /* Pointer to HtmlTree.aImage value */
    Tcl_HashEntry *pEntry;            /* Cache entry */
    int newentry;                     /* True to add a new cache entry */
    int w, h;                         /* Intrinsic width and height of image */
    Tcl_Obj *pRet = 0;                /* Return value */
    Tcl_Interp *interp = pTree->interp;

    /* Look up the entry in HtmlTree.aImage for this image. If one does not
     * exist, create it. Even images that are never scaled need to have an
     * entry - this is how we delete them when they are no longer required.
     */
    pEntry = Tcl_CreateHashEntry(&pTree->aImage, zImage, &newentry);
    if (newentry) {
        Tk_Image img;
        img = Tk_GetImage(interp, pTree->win, zImage, imageChanged, 0);
        if (!img) {
            Tcl_DeleteHashEntry(pEntry);
            return 0;
        }

        pImage = (HtmlScaledImage *)ckalloc(sizeof(HtmlScaledImage));
        pImage->pImageName = Tcl_NewStringObj(zImage, -1);
        Tcl_IncrRefCount(pImage->pImageName);
        pImage->image = img;
        pImage->scaled_image = 0;
        pImage->pScaledImageName = 0;

        Tcl_SetHashValue(pEntry, pImage);
    }
    pImage = Tcl_GetHashValue(pEntry);

    /* Find the intrinsic size of the image. If *pWidth or *pHeight are -1,
     * then set them to the value of the intrinsic dimension.
     */
    Tk_SizeOfImage(pImage->image, &w, &h);
    if (*pWidth<0) *pWidth = w;
    if (*pHeight<0) *pHeight = h;

    if (*pWidth==w && *pHeight==h) {
        /* If the requested dimensions match the intrinsic dimensions,
         * just return the name of the original image.
         */
        pRet = pImage->pImageName;
    } else {
        if (pImage->scaled_image) {
            /* Otherwise, check the dimensions of the cached scaled image,
             * if one exists. If these match the requested dimensions, we
             * can return the name of the cached scaled image. Otherwise,
             * destroy the cached image. We will create a new scaled copy
             * below.
             */
            int sw, sh;         /* Width and height of cached scaled image */
            Tk_SizeOfImage(pImage->scaled_image, &sw, &sh);
            if (*pWidth==sw && *pHeight==sh) {
                pRet = pImage->pScaledImageName;
            } else {
                Tk_DeleteImage(interp, Tcl_GetString(pImage->pScaledImageName));
                Tcl_DecrRefCount(pImage->pScaledImageName);
                pImage->scaled_image = 0;
                pImage->pScaledImageName = 0;
            }
        }
    }

    if (!pRet) {
        /* If we get here, then we need to create a scaled copy of the
         * original image to return.
         */
        Tk_PhotoHandle photo;

        photo = Tk_FindPhoto(interp, Tcl_GetString(pImage->pImageName));
        if (photo) { 
            int x, y;                /* Iterator variables */
            int sw, sh;              /* Width and height of scaled image */
            CONST char *zScaled;
            Tk_Image scaled;
            Tk_PhotoHandle scaled_photo;
            Tk_PhotoImageBlock block;
            Tk_PhotoImageBlock scaled_block;

            sw = *pWidth;
            sh = *pHeight;

            /* Create a new photo image to write the scaled data to */
            Tcl_Eval(interp, "image create photo");
            pImage->pScaledImageName = Tcl_GetObjResult(interp);
            Tcl_IncrRefCount(pImage->pScaledImageName);
            zScaled = Tcl_GetString(pImage->pScaledImageName);
            scaled = Tk_GetImage(interp, pTree->win, zScaled, imageChanged, 0);
            pImage->scaled_image = scaled;
            scaled_photo = Tk_FindPhoto(interp, zScaled);

            Tk_PhotoGetImage(photo, &block);
            scaled_block.pixelPtr = ckalloc(sw * sh * 4);
            scaled_block.width = sw;
            scaled_block.height = sh;
            scaled_block.pitch = sw*4;
            scaled_block.pixelSize = 4;
            scaled_block.offset[0] = 0;
            scaled_block.offset[1] = 1;
            scaled_block.offset[2] = 2;
            scaled_block.offset[3] = 3;

            for (x=0; x<sw; x++) {
                int orig_x = ((x * w) / sw);
                for (y=0; y<sh; y++) {
                    unsigned char *zOrig;
                    unsigned char *zScale;
                    int orig_y = ((y * h) / sh);

                    zOrig = &block.pixelPtr[
                        orig_x * block.pixelSize + orig_y * block.pitch];
                    zScale = &scaled_block.pixelPtr[
                        x * scaled_block.pixelSize + y * scaled_block.pitch];

                    zScale[0] = zOrig[block.offset[0]];
                    zScale[1] = zOrig[block.offset[1]];
                    zScale[2] = zOrig[block.offset[2]];
                    zScale[3] = zOrig[block.offset[3]];
                }
            }
            Tk_PhotoPutBlock(interp,scaled_photo,&scaled_block,0,0,sw,sh,0);
            ckfree(scaled_block.pixelPtr);
        } else {
            /* Failed to get the photo-handle. This might happen because
             * someone passed an image of type bitmap to the widget. Since
             * the internal pixel data is not available, we can't scale the
             * image. Just return the original.
             */
            pRet = pImage->pImageName;
        }
    }

    return pRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlXImageToImage --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj *HtmlXImageToImage(pTree, pXImage, w, h)
    HtmlTree *pTree;
    XImage *pXImage;
    int w;
    int h;
{
    Tcl_Interp *interp = pTree->interp;

    Tcl_Obj *pImage;
    Tk_PhotoHandle photo;
    Tk_PhotoImageBlock block;
    int x;
    int y;
    unsigned long redmask, redshift;
    unsigned long greenmask, greenshift;
    unsigned long bluemask, blueshift;
    Visual *pVisual;

    Tcl_Eval(interp, "image create photo");
    pImage = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(pImage);

    block.pixelPtr = ckalloc(w * h * 4);
    block.width = w;
    block.height = h;
    block.pitch = w*4;
    block.pixelSize = 4;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;

    pVisual = Tk_Visual(pTree->win);

    redmask = pVisual->red_mask;
    bluemask = pVisual->blue_mask;
    greenmask = pVisual->green_mask;
    for (redshift=0; !((redmask>>redshift)&0x000000001); redshift++);
    for (greenshift=0; !((greenmask>>greenshift)&0x00000001); greenshift++);
    for (blueshift=0; !((bluemask>>blueshift)&0x00000001); blueshift++);

    for (x=0; x<w; x++) {
        for (y=0; y<h; y++) {
            char *pOut = &block.pixelPtr[x*block.pixelSize + y*block.pitch];
            unsigned long pixel = XGetPixel(pXImage, x, y);

            pOut[0] = (pixel&redmask)>>redshift;
            pOut[1] = (pixel&greenmask)>>greenshift;
            pOut[2] = (pixel&bluemask)>>blueshift;
            pOut[3] = 0xFF;
        }
    }

    photo = Tk_FindPhoto(interp, Tcl_GetString(pImage));
    Tk_PhotoPutBlock(interp, photo, &block, 0, 0, w, h, 0);
    ckfree(block.pixelPtr);

    return pImage;
}
