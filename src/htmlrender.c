/*
** Routines used to render HTML onto the screen
*/
#define panic bogus_procedure_name
#include <tkPort.h>
#undef panic
#include <default.h>
#include <tclInt.h>
#include "htmlrender.h"

#if INTERFACE

#define NChar 300

/*
** In order to help the X Server go faster, text drawing requests are
** batched as much as possible.  The following structure holds state
** information used for the batching.
*/
struct HtmlDrawer {
  HtmlWidget *htmlPtr;     /* The HTML widget which we are rendering */
  int n;                   /* Number of characters in z[] */
  HtmlStyle style;         /* All text rendered in this style */
  int x, y;                /* Starting position of the text */
  int xEnd;                /* Where next character would go */
  Drawable drawable;       /* Render text on this object */
  int xOffset;             /* The left edge of drawable */
  int yOffset;             /* The top edge of drawable */
  char z[NChar];           /* Text of buffer characters */
};

#endif

/*
** Initialize a new drawing structure.  The calling function should
** have allocated the drawing structure for us.
*/
void HtmlStartDrawer(
  HtmlWidget *htmlPtr,
  HtmlDrawer *pDrawer,
  Drawable drawable,
  int x, 
  int y
){
  pDrawer->htmlPtr = htmlPtr;
  pDrawer->n = 0;
  pDrawer->drawable = drawable;
  pDrawer->xOffset = x;
  pDrawer->yOffset = y;
}

/*
** Flush all text that might be accumulated in the drawing structure
*/
void HtmlFlushDrawing(
  HtmlDrawer *pDrawer  
){
  Tk_Font font;
  GC gc;
  int n;

  if( pDrawer->n<=0 ) return;
  font = HtmlGetFont(pDrawer->htmlPtr, pDrawer->style.font);
  gc = HtmlGetGC(pDrawer->htmlPtr, pDrawer->style.color, pDrawer->style.font);
  for(n=pDrawer->n; n>0 && pDrawer->z[n-1]==' '; n--){}
  if( n>0 ){
    Tk_DrawChars(pDrawer->htmlPtr->display,
                 pDrawer->drawable,
                 gc, font,
                 pDrawer->z, n,
                 pDrawer->x - pDrawer->xOffset,
                 pDrawer->y - pDrawer->yOffset);
  }
  pDrawer->n = 0;
}

/*
** Render a single element of Html
*/
void HtmlDrawElement(
  HtmlDrawer *pDrawer,
  HtmlElement *p
){
  switch( p->type ){
    case Html_Text:
      if( pDrawer->n>0 ){
        if( p->y != pDrawer->y 
        ||  p->x != pDrawer->xEnd
        ||  p->style != pDrawer->style
        ||  pDrawer->n + p->count >= NChar
        ){
          HtmlFlushDrawing(pDrawer);
        }
      }
      if( pDrawer->n==0 ){
        pDrawer->x = p->x;
        pDrawer->y = p->y;
        pDrawer->style = p->style;
        pDrawer->xEnd = p->x + p->w;
        pDrawer->n = p->count;
        strncpy(pDrawer->z, p->u.zText, p->count);
      }else{
        strncpy(&pDrawer->z[pDrawer->n], p->u.zText, p->count);
        pDrawer->n += p->count;
        pDrawer->xEnd = p->x + p->w;
      }
      break;
    case Html_Space:
      if( pDrawer->n>0 ){
        if( p->y != pDrawer->y 
        ||  p->x != pDrawer->xEnd
        ||  p->style != pDrawer->style
        ||  pDrawer->n + p->count >= NChar
        ){
          break;
        }
        if( p->style.flags & STY_Preformatted ){
          if( p->u.zText[p->count-1]=='\n' ){
            HtmlFlushDrawing(pDrawing);
          }else{
            int i;
            for(i=0; i<p->count; i++){
              pDrawing->z[pDrawing->n++] = ' ';
            }
            pDrawing->xEnd = p->x + p->w;
          }
        }else{
          int i;
          for(i=0; i<p->count; i++){
            pDrawing->z[pDrawing->n++] = ' ';
          }
          pDrawing->xEnd = p->x + p->w;
        }
      }
      break;
    default:
      break;
  }
}
