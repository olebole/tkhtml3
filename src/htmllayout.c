/*
** This file contains the code used to position elements of the
** HTML file on the screen.
** $Revision: 1.5 $
**
** Copyright (C) 1997,1998 D. Richard Hipp
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
** 
** You should have received a copy of the GNU Library General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@acm.org
**   http://www.hwaci.com/drh/
*/
#include <tk.h>
#include <stdlib.h>
#include <string.h>
#include "htmllayout.h"


/*
** Push a new margin onto the given margin stack.
*/
void HtmlPushMargin(
  HtmlMargin **ppMargin,  /* The margin stack onto which to push */
  int indent,             /* The indentation for the new margin */
  int bottom              /* The margin expires at this Y coordinate */
){
  HtmlMargin *pNew = (HtmlMargin*)ckalloc( sizeof(HtmlMargin) );
  pNew->pNext = *ppMargin;
  if( pNew->pNext ){
    pNew->indent = indent + pNew->pNext->indent;
    TestPoint(0);
  }else{
    pNew->indent = indent;
    TestPoint(0);
  }
  pNew->bottom = bottom;
  *ppMargin = pNew;
}

/*
** Pop a margin off of the given margin stack.
*/
void HtmlPopMargin(HtmlMargin **ppMargin){
  if( *ppMargin ){
    HtmlMargin *pOld = *ppMargin;
    *ppMargin = pOld->pNext;
    ckfree((char*)pOld);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
}

/*
** Pop all expired margins from the stack.
*/
static void PopExpiredMargins(HtmlMargin **ppMarginStack, int y){
  while( *ppMarginStack && (**ppMarginStack).bottom <= y ){
    HtmlPopMargin(ppMarginStack);
    TestPoint(0);
  }
}

/*
** This routine gathers as many tokens as will fit on one line.
**
** The candidate tokens begin with pStart and go thru the end of
** the list or to pEnd, whichever comes first.  The first token
** at the start of the next line is returned.  NULL is returned if
** we exhaust data.
**
** "width" is the maximum allowed width of the line.  The actual
** width is returned in *actualWidth.  The actual width does not
** include any trailing spaces.  Sometimes the actual width will
** be greater than the maximum width.  This will happen, for example,
** for text enclosed in <pre>..</pre> that has lines longer than
** the width of the page.
**
** If the list begins with text, at least one token is returned,
** even if that one token is longer than the allowed line length.
** But if the list begins with some kind of break markup (possibly
** preceded by white space) then the returned list may be empty.
**
** The "x" coordinates of all elements are set assuming that the line
** begins at 0.  The calling routine should adjust these coordinates
** to position the line horizontally.  (The FixLine() procedure does
** this.)  Note that the "x" coordinate of <li> elements will be negative.
** Text within <dt>..</dt> might also have a negative "x" coordinate.
*/
static HtmlElement *GetLine(
  HtmlLayoutContext *pLC,      /* The complete layout context.  */
  HtmlElement *pStart,         /* First token on new line */
  HtmlElement *pEnd,           /* End of line.  Might be NULL */
  int width,                   /* How much space is on this line */
  int *actualWidth             /* Return space actually required */
){
  int x;                       /* Current X coordinate */
  int spaceWanted = 0;         /* Add this much space before next token */
  HtmlElement *p;              /* For looping over tokens */
  HtmlElement *lastBreak = 0;  /* Last line-break opportunity */
  int isEmpty = 1;             /* True if link contains nothing */
  int origin;                  /* Initial value of "x" */
  
  *actualWidth = 0;
  p = pStart;
  while( p && p!=pEnd && (p->base.style.flags & STY_Invisible)!=0 ){
    p = p->pNext;
    TestPoint(0);
  }
  if( p->base.style.flags & STY_DT ){
    origin = -HTML_INDENT;
    TestPoint(0);
  }else{
    origin = 0;
    TestPoint(0);
  }
  x = origin;
  if( p && p!=pEnd && p->base.type==Html_LI ){
    p->li.x = x - (HTML_INDENT*1)/3;
    isEmpty = 0;
    *actualWidth = 1;
    p = p->pNext;
    while( p && (p->base.type==Html_Space || p->base.type==Html_P) ){
      p = p->pNext;
    }
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  for(; p && p!=pEnd; p=p->pNext){
    if( p->base.style.flags & STY_Invisible ){
      TestPoint(0);
      continue;
    }
    switch( p->base.type ){
      case Html_Text:
        p->text.x = x + spaceWanted;
        if( (p->base.style.flags & STY_Preformatted) == 0 ){
          if( lastBreak && x + spaceWanted + p->text.w > width ){
            TestPoint(0);
            return lastBreak;
          }
          TestPoint(0);
        }else{
          TestPoint(0);
        }
        TRACE(HtmlTrace_GetLine2, ("Place token %s at x=%d w=%d\n",
           HtmlTokenName(p), p->text.x, p->text.w));
        x += p->text.w + spaceWanted;
        isEmpty = 0;
        spaceWanted = 0;
        break;

      case Html_Space:
        if( p->base.style.flags & STY_Preformatted ){
          if( p->base.flags & HTML_NewLine ){
            *actualWidth = x<=0 ? 1 : x;
            TestPoint(0);
            return p->pNext;
          }
          x += p->space.w * p->base.count;
          TestPoint(0);
        }else{
          int w;
          if( (p->base.style.flags & STY_NoBreak)==0 ){
            lastBreak = p->pNext;
            *actualWidth = x<=0 && !isEmpty ? 1 : x;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
          w = p->space.w;
          if( spaceWanted < w && x>origin ){
            spaceWanted = w;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
        }
        break;

      case Html_IMG:
        switch( p->image.align ){
          case IMAGE_ALIGN_Left:
          case IMAGE_ALIGN_Right:
            *actualWidth = x<=0 && !isEmpty ? 1 : x;
            TestPoint(0);
            return p;
          default:
            TestPoint(0);
            break;
        }
        p->image.x = x + spaceWanted;
        if( (p->base.style.flags & STY_Preformatted) == 0 ){
          if( lastBreak && x + spaceWanted + p->image.w > width ){
            TestPoint(0);
            return lastBreak;
          }
          TestPoint(0);
        }else{
          TestPoint(0);
        }
        TRACE(HtmlTrace_GetLine2, ("Place in-line image %s at x=%d w=%d\n",
           HtmlTokenName(p), p->image.x, p->image.w));
        x += p->image.w + spaceWanted;
        spaceWanted = 0;
        isEmpty = 0;
        break;

      case Html_APPLET:
      case Html_INPUT:
      case Html_TEXTAREA:
        p->input.x = x + spaceWanted + p->input.padLeft;
        if( (p->base.style.flags & STY_Preformatted) == 0 ){
          if( lastBreak && x + spaceWanted + p->input.w > width ){
            TestPoint(0);
            return lastBreak;
          }
          TestPoint(0);
        }else{
          TestPoint(0);
        }
        TRACE(HtmlTrace_GetLine2, ("Place token %s at x=%d w=%d\n",
           HtmlTokenName(p), p->input.x, p->input.w));
        x = p->input.x + p->input.w;
        spaceWanted = 0;
        isEmpty = 0;
        break;

      case Html_EndTEXTAREA:
        if( p->ref.pOther ){
          /* HtmlResetTextarea(pLC->htmlPtr, p->ref.pOther); */
          TestPoint(0);
        }else{
          TestPoint(0);
        }
        break;

      case Html_DD:
        if( p->ref.pOther==0 ) break;
        if( p->ref.pOther->list.compact==0 || x + spaceWanted >= 0 ){
          *actualWidth = x<=0 && !isEmpty ? 1 : x;
          TestPoint(0);
          return p;
        }
        x = 0;
        spaceWanted = 0;
        TestPoint(0);
        break;

      case Html_WBR:
        *actualWidth = x<=0 && !isEmpty ? 1 : x;
        if( x + spaceWanted >= width ){
          TestPoint(0);
          return p->pNext;
        }else{
          TestPoint(0);
          lastBreak = p->pNext;
        }
        break;

      case Html_ADDRESS:
      case Html_EndADDRESS:
      case Html_EndAPPLET:
      case Html_BLOCKQUOTE:
      case Html_EndBLOCKQUOTE:
      case Html_BODY:
      case Html_EndBODY:
      case Html_BR:
      case Html_CAPTION:
      case Html_EndCAPTION:
      case Html_CENTER:
      case Html_EndCENTER:
      case Html_EndDD:
      case Html_DIV:
      case Html_EndDIV:
      case Html_DL:
      case Html_EndDL:
      case Html_DT:
      case Html_H1:
      case Html_EndH1:
      case Html_H2:
      case Html_EndH2:
      case Html_H3:
      case Html_EndH3:
      case Html_H4:
      case Html_EndH4:
      case Html_H5:
      case Html_EndH5:
      case Html_H6:
      case Html_EndH6:
      case Html_EndHTML:
      case Html_HR:
      case Html_LI:
      case Html_LISTING:
      case Html_EndLISTING:
      case Html_MENU:
      case Html_EndMENU:
      case Html_OL:
      case Html_EndOL:
      case Html_P:
      case Html_EndP:
      case Html_PRE:
      case Html_EndPRE:
      case Html_TABLE:
      case Html_EndTABLE:
      case Html_TD:
      case Html_EndTD:
      case Html_TH:
      case Html_EndTH:
      case Html_TR:
      case Html_EndTR:
      case Html_UL:
      case Html_EndUL:
        *actualWidth = x<=0 && !isEmpty ? 1 : x;
        TestPoint(0);
        return p;

      default:
        TestPoint(0);
        break;
    }
  }
  *actualWidth = x<=0 && !isEmpty ? 1 : x;
  return p;
}

/*
** Set the y coordinate for every anchor in the given list
*/
static void FixAnchors(HtmlElement *p, HtmlElement *pEnd, int y){
  while( p && p!=pEnd ){
    if( p->base.type==Html_A ){
      p->anchor.y = y;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    p = p->pNext;
  }
}

/*
** This routine computes the X and Y coordinates for all elements of
** a line that has been gathered using GetLine() above.   It also figures
** the ascent and descent for in-line images.
**
** The value returned is the Y coordinate of the bottom edge of the
** new line.  The X coordinates are computed by adding the left margin
** plus any extra space needed for centering or right-justification.
*/
static int FixLine(
  HtmlElement *pStart,   /* Start of tokens for this line */
  HtmlElement *pEnd,     /* First token past end of this line.  Maybe NULL */
  int bottom,            /* Put the top of this line here */
  int width,             /* This is the space available to the line */
  int actualWidth,       /* This is the actual width needed by the line */
  int leftMargin,        /* The current left margin */
  int *maxX              /* Write maximum X coordinate of ink here */
){
  int dx;                /* Amount by which to increase all X coordinates */
  int maxAscent;         /* Maximum height above baseline */
  int maxTextAscent;     /* Maximum height above baseline for text */
  int maxDescent;        /* Maximum depth below baseline */
  int ascent, descent;   /* Computed ascent and descent for one element */
  HtmlElement *p;        /* For looping */
  int y;                 /* Y coordinate of the baseline */
  int dy2center;         /* Distance from baseline to text font center */
  int max = 0; 

  if( actualWidth>0 ){
    for(p=pStart; p && p!=pEnd && p->base.type!=Html_Text; p=p->pNext){}
    if( p==pEnd || p==0 ) p = pStart;
    if( p->base.style.align == ALIGN_Center ){
      dx = leftMargin + (width - actualWidth)/2;
      TestPoint(0);
    }else if( p->base.style.align == ALIGN_Right ){
      dx = leftMargin + (width - actualWidth);
      TestPoint(0);
    }else{
      dx = leftMargin;
      TestPoint(0);
    }
    maxAscent = maxTextAscent = 0;
    for(p=pStart; p && p!=pEnd; p=p->pNext){
      int ss;
      if( p->base.style.flags & STY_Invisible ){
        TestPoint(0);
        continue;
      }
      switch( p->base.type ){
        case Html_Text:
          p->text.x += dx;
          max = p->text.x + p->text.w;
          ss = p->base.style.subscript;
          if( ss != 0 ){
            int ascent = p->text.ascent;
            int delta = (ascent + p->text.descent)/2;
            ascent += delta;
            p->text.y = -delta;
            if( ascent > maxAscent ){ TestPoint(0); maxAscent = ascent; }
            if( ascent > maxTextAscent ){ TestPoint(0); maxTextAscent = ascent;}
          }else{
            p->text.y = 0;
            if( p->text.ascent > maxAscent ){ 
              TestPoint(0); 
              maxAscent = p->text.ascent;
            }
            if( p->text.ascent > maxTextAscent ){
              TestPoint(0);
              maxTextAscent = p->text.ascent;
            }
          }
          break;
        case Html_Space:
          if( p->space.ascent > maxAscent ){ 
            TestPoint(0); 
            maxAscent = p->space.ascent;
          }
          break;
        case Html_LI:
          p->li.x += dx;
          if( p->li.x > max ){
            TestPoint(0); 
            max = p->li.x; 
          }else{
            TestPoint(0);
          }
          break;
        case Html_IMG:
          p->image.x += dx;
          max = p->image.x + p->image.w;
          switch( p->image.align ){
            case IMAGE_ALIGN_Middle:
              p->image.descent = p->image.h/2;
              p->image.ascent = p->image.h - p->image.descent;
              if( p->image.ascent > maxAscent ){
                maxAscent = p->image.ascent;
                TestPoint(0);
              }else{
                TestPoint(0);
              }
              break;
            case IMAGE_ALIGN_AbsMiddle:
              dy2center = (p->image.textDescent - p->image.textAscent)/2;
              p->image.descent = p->image.h/2 + dy2center;
              p->image.ascent = p->image.h - p->image.descent;
              if( p->image.ascent > maxAscent ){
                maxAscent = p->image.ascent;
                TestPoint(0);
              }else{
                TestPoint(0);
              }
              break;
            case IMAGE_ALIGN_Bottom:
              p->image.descent = 0;
              p->image.ascent = p->image.h;
              if( p->image.ascent > maxAscent ){
                maxAscent = p->image.ascent;
                TestPoint(0);
              }else{
                TestPoint(0);
              }
              break;
            case IMAGE_ALIGN_AbsBottom:
              p->image.descent = p->image.textDescent;
              p->image.ascent = p->image.h - p->image.descent;
              if( p->image.ascent > maxAscent ){
                maxAscent = p->image.ascent;
                TestPoint(0);
              }else{
                TestPoint(0);
              }
              break;
            default:
              TestPoint(0);
              break;
          }
          break;
        case Html_TEXTAREA:
        case Html_INPUT:
        case Html_APPLET:
          p->input.x += dx;
          max = p->input.x + p->input.w;
          dy2center = (p->input.textDescent - p->input.textAscent)/2;
          p->input.y = dy2center - p->input.h/2;
          ascent = -p->input.y;
          if( ascent > maxAscent ){
            maxAscent = ascent;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
          break;
        default:
          /* Shouldn't happen */
          break;
      }
    }
    *maxX = max;
    y = maxAscent + bottom;
    maxDescent = 0;
    for(p=pStart; p && p!=pEnd; p=p->pNext){
      if( p->base.style.flags & STY_Invisible ){
        TestPoint(0);
        continue;
      }
      switch( p->base.type ){
        case Html_Text:
          p->text.y += y;
          if( p->text.descent > maxDescent ){
            maxDescent = p->text.descent;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
          break;
        case Html_LI:
          p->li.y = y;
          if( p->li.descent > maxDescent ){
            maxDescent = p->li.descent;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
          break;
        case Html_IMG:
          p->image.y = y;
          switch( p->image.align ){
            case IMAGE_ALIGN_Top:
              p->image.ascent = maxAscent;
              p->image.descent = p->image.h - maxAscent;
              TestPoint(0);
              break;
            case IMAGE_ALIGN_TextTop:
              p->image.ascent = maxTextAscent;
              p->image.descent = p->image.h - maxTextAscent;
              TestPoint(0);
              break;
            default:
              TestPoint(0);
              break;
          }
          if( p->image.descent > maxDescent ){
            maxDescent = p->image.descent;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
          break;
        case Html_INPUT:
        case Html_TEXTAREA:
        case Html_APPLET:
          descent = p->input.y + p->input.h;
          p->input.y += y;
          if( descent > maxDescent ){
            maxDescent = descent;
            TestPoint(0);
          }else{
            TestPoint(0);
          }
          break;
        default:
          /* Shouldn't happen */
          break;
      }
    }
    TRACE(HtmlTrace_FixLine, 
       ("Setting baseline to %d. bottom=%d ascent=%d descent=%d dx=%d\n",
       y, bottom, maxAscent, maxDescent, dx));
  }else{
    maxDescent = 0;
    y = bottom;
    TestPoint(0);
  }
  return y + maxDescent;
}

/*
** Increase the headroom to create a paragraph break at the current token
*/
static void Paragraph(
  HtmlLayoutContext *pLC,
  HtmlElement *p
){
  int headroom;

  if( p==0 ){ TestPoint(0); return; }
  if( p->base.type==Html_Text ){
    headroom = p->text.ascent + p->text.descent;
    TestPoint(0);
  }else if( p->pNext && p->pNext->base.type==Html_Text ){
    headroom = p->pNext->text.ascent + p->pNext->text.descent;
    TestPoint(0);
  }else{
    Tk_FontMetrics fontMetrics;
    Tk_Font font = HtmlGetFont(pLC->htmlPtr, p->base.style.font);
    Tk_GetFontMetrics(font, &fontMetrics);
    headroom = fontMetrics.descent + fontMetrics.ascent;
    TestPoint(0);
  }
  if( pLC->headRoom < headroom && pLC->bottom > pLC->top ){
    pLC->headRoom = headroom;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
}

/*
** Compute the current margins for layout.  Three values are returned:
**
**    *pY       The top edge of the area in which we can put ink.  This
**              takes into account any requested headroom.
**
**    *pX       The left edge of the inkable area.  The takes into account
**              any margin requests active at vertical position specified
**              in pLC->bottom.
**
**    *pW       The width of the inkable area.  This takes into account
**              an margin requests that are active at the vertical position
**              pLC->bottom.
**
*/
static void ComputeMargins(
  HtmlLayoutContext *pLC,    /* The current layout context */
  int *pX,                   /* Put the left edge here */
  int *pY,                   /* Put the top edge here */
  int *pW                    /* Put the width here */
){
  int x, y, w;

  y = pLC->bottom + pLC->headRoom;
  PopExpiredMargins(&pLC->leftMargin, pLC->bottom);
  PopExpiredMargins(&pLC->rightMargin, pLC->bottom);
  w = pLC->pageWidth - pLC->right;
  if( pLC->leftMargin ){
    x = pLC->leftMargin->indent + pLC->left;
    TestPoint(0);
  }else{
    x = pLC->left;
    TestPoint(0);
  }
  w -= x;
  if( pLC->rightMargin ){
    w -= pLC->rightMargin->indent;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  *pX = x;
  *pY = y;
  *pW = w;
}


/*
** Clear a wrap-around obstacle.  The second option determines the
** precise behavior.
**
**    CLEAR_Left        Clear all obstacles on the left.
**
**    CLEAR_Right       Clear all obstacles on the right.
**
**    CLEAR_Both        Clear all obstacles on both sides.
**
**    CLEAR_First       Clear only the first obsticle on either side.
*/
#define CLEAR_Left  0
#define CLEAR_Right 1
#define CLEAR_Both  2
#define CLEAR_First 3
static void ClearObstacle(HtmlLayoutContext *pLC, int mode){
  int newBottom = pLC->bottom;

  switch( mode ){
    case CLEAR_Both:
      ClearObstacle(pLC,CLEAR_Left);
      ClearObstacle(pLC,CLEAR_Right);
      TestPoint(0);
      break;

    case CLEAR_Left:
      while( pLC->leftMargin && pLC->leftMargin->bottom < LARGE_NUMBER ){
        newBottom = pLC->leftMargin->bottom;
        HtmlPopMargin(&pLC->leftMargin);
        TestPoint(0);
      }
      if( newBottom > pLC->bottom + pLC->headRoom ){
        pLC->headRoom = 0;
        TestPoint(0);
      }else{
        pLC->headRoom = newBottom - pLC->bottom;
        TestPoint(0);
      }
      pLC->bottom = newBottom;
      PopExpiredMargins(&pLC->rightMargin, pLC->bottom);
      break;

    case CLEAR_Right:
      while( pLC->rightMargin && pLC->rightMargin->bottom < LARGE_NUMBER ){
        newBottom = pLC->rightMargin->bottom;
        HtmlPopMargin(&pLC->rightMargin);
        TestPoint(0);
      }
      if( newBottom > pLC->bottom + pLC->headRoom ){
        pLC->headRoom = 0;
        TestPoint(0);
      }else{
        pLC->headRoom = newBottom - pLC->bottom;
        TestPoint(0);
      }
      pLC->bottom = newBottom;
      PopExpiredMargins(&pLC->leftMargin, pLC->bottom);
      break;

    case CLEAR_First:
      if( pLC->leftMargin && pLC->leftMargin->bottom < LARGE_NUMBER ){
        if( pLC->rightMargin 
         && pLC->rightMargin->bottom < pLC->leftMargin->bottom
        ){
          newBottom = pLC->rightMargin->bottom;
          HtmlPopMargin(&pLC->rightMargin);
          TestPoint(0);
        }else{
          newBottom = pLC->leftMargin->bottom;
          HtmlPopMargin(&pLC->leftMargin);
          TestPoint(0);
        }
      }else if( pLC->rightMargin && pLC->rightMargin->bottom < LARGE_NUMBER ){
        newBottom = pLC->rightMargin->bottom;
        HtmlPopMargin(&pLC->rightMargin);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
      if( newBottom > pLC->bottom + pLC->headRoom ){
        pLC->headRoom = 0;
        TestPoint(0);
      }else{
        pLC->headRoom = newBottom - pLC->bottom;
        TestPoint(0);
      }
      pLC->bottom = newBottom;
      break;
  }
}

/*
** Break markup is any kind of markup that might force a line-break. This
** routine handles a single element of break markup and returns a pointer
** to the first element past that markup.  If p doesn't point to break
** markup, then p is returned.  If p is an incomplete table (a <TABLE>
** that lacks a </TABLE>), then NULL is returned.
*/
static HtmlElement *DoBreakMarkup(
  HtmlLayoutContext *pLC,
  HtmlElement *p
){
  HtmlElement *pNext = p->pNext;
  char *z;
  int x, y, w;

  switch( p->base.type ){
    case Html_A:
      p->anchor.y = pLC->bottom;
      TestPoint(0);
      break;

    case Html_BLOCKQUOTE:
      HtmlPushMargin(&pLC->leftMargin, HTML_INDENT, LARGE_NUMBER);
      HtmlPushMargin(&pLC->rightMargin, HTML_INDENT, LARGE_NUMBER);
      Paragraph(pLC, p);
      TestPoint(0);
      break;
    case Html_EndBLOCKQUOTE:
      HtmlPopMargin(&pLC->leftMargin);
      HtmlPopMargin(&pLC->rightMargin);
      Paragraph(pLC, p);
      TestPoint(0);
      break;

    case Html_IMG:
      switch( p->image.align ){
        case IMAGE_ALIGN_Left:
          ComputeMargins(pLC, &x, &y, &w);
          p->image.x = x;
          p->image.y = y;
          p->image.ascent = 0;
          p->image.descent = p->image.h;
          HtmlPushMargin(&pLC->leftMargin, p->image.w + 2, y + p->image.h);
          SETMAX( pLC->maxY, y + p->image.h );
          SETMAX( pLC->maxX, x + p->image.w );
          break;
        case IMAGE_ALIGN_Right:
          ComputeMargins(pLC, &x, &y, &w);
          p->image.x = x + w - p->image.w;
          p->image.y = y;
          p->image.ascent = 0;
          p->image.descent = p->image.h;
          HtmlPushMargin(&pLC->rightMargin, p->image.w + 2, y + p->image.h);
          SETMAX( pLC->maxY, y + p->image.h );
          SETMAX( pLC->maxX, x + p->image.w );
          break;
        default:
          TestPoint(0);
          pNext = p;
          break;
      }
      break;

    
    case Html_PRE:
      /* Skip space tokens thru the next newline. */
      while( pNext->base.type==Html_Space ){
        HtmlElement *pThis = pNext;
        pNext = pNext->pNext;
        if( pThis->base.flags & HTML_NewLine ){ TestPoint(0); break; }
        TestPoint(0);
      }
      Paragraph(pLC,p);
      break;

    case Html_UL:
    case Html_MENU:
    case Html_DIR:
    case Html_OL:
      if( p->list.compact==0 ){
        Paragraph(pLC,p);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
      HtmlPushMargin(&pLC->leftMargin, HTML_INDENT, LARGE_NUMBER);
      break;

    case Html_EndOL:
    case Html_EndUL:
    case Html_EndMENU:
    case Html_EndDIR:
      if( p->ref.pOther ){
        HtmlPopMargin(&pLC->leftMargin);
        if( !p->ref.pOther->list.compact ){
          Paragraph(pLC,p);
          TestPoint(0);
        }else{
          TestPoint(0);
        }
      }else{
        TestPoint(0);
      }
      break;

    case Html_DL:
      Paragraph(pLC,p);
      HtmlPushMargin(&pLC->leftMargin, HTML_INDENT, LARGE_NUMBER);
      TestPoint(0);
      break;

    case Html_EndDL:
      HtmlPopMargin(&pLC->leftMargin);
      Paragraph(pLC,p);
      TestPoint(0);
      break;

    case Html_HR:
      {
        int zl, wd;

        z = HtmlMarkupArg(p, "size", "3");
        if( z ){
          p->hr.h = atoi(z);
          if( p->hr.h<1 ) p->hr.h = 3;
        }else{
          p->hr.h = 3;
        }
        p->hr.is3D = HtmlMarkupArg(p, "noshade", 0)==0;
        ComputeMargins(pLC, &x, &y, &w);
        p->hr.y = y;
        y += p->hr.h + 1;
        p->hr.x = x;
        z = HtmlMarkupArg(p, "width", "100%");
        zl = strlen(z);
        if( zl>0 && z[zl-1]=='%' ){
          wd = (atoi(z)*w)/100;
        }else{
          wd = atoi(z);
        }
        p->hr.w = wd;
        switch( p->base.style.align ){
          case ALIGN_Center:
          case ALIGN_None:
            p->hr.x += (w - wd)/2;
            TestPoint(0);
            break;
          case ALIGN_Right:
            p->hr.x += (w - wd);
            TestPoint(0);
            break;
          default:
            TestPoint(0);
            break;
        }
        SETMAX( pLC->maxY, y);
        SETMAX( pLC->maxX, wd + p->hr.x );
        pLC->bottom = y;
        pLC->headRoom = 0;
      }
      TestPoint(0);
      break;

    case Html_ADDRESS:
    case Html_EndADDRESS:
    case Html_CENTER:
    case Html_EndCENTER:
    case Html_DIV:
    case Html_EndDIV:
    case Html_H1:
    case Html_EndH1:
    case Html_H2:
    case Html_EndH2:
    case Html_H3:
    case Html_EndH3:
    case Html_H4:
    case Html_EndH4:
    case Html_H5:
    case Html_EndH5:
    case Html_H6:
    case Html_EndH6:
    case Html_P:
    case Html_EndP:
    case Html_EndPRE:
      Paragraph(pLC, p);
      TestPoint(0);
      break;

    case Html_TABLE:
      pNext = HtmlTableLayout(pLC, p);
      TestPoint(0);
      break;

    case Html_BR:
      z = HtmlMarkupArg(p, "clear",0);
      if( z ){
        if( stricmp(z,"left")==0 ){
          ClearObstacle(pLC, CLEAR_Left);
          TestPoint(0);
        }else if( stricmp(z,"right")==0 ){
          ClearObstacle(pLC, CLEAR_Right);
          TestPoint(0);
        }else{
          ClearObstacle(pLC, CLEAR_Both);
          TestPoint(0);
        }
      }else{
        TestPoint(0);
      }
      break;

    /* All of the following tags need to be handed to the GetLine() routine */
    case Html_Text:
    case Html_Space:
    case Html_LI:
    case Html_INPUT:
    case Html_TEXTAREA:
    case Html_APPLET:
      pNext = p;
      TestPoint(0);
      break;

    default:
      TestPoint(0);
      break;
  }
  return pNext;
}

/*
** Return TRUE (non-zero) if we are currently wrapping text around
** one or more images.
*/
static int InWrapAround(HtmlLayoutContext *pLC){
  if( pLC->leftMargin && pLC->leftMargin->bottom < LARGE_NUMBER ){
    TestPoint(0);
    return 1;
  }
  if( pLC->rightMargin && pLC->rightMargin->bottom < LARGE_NUMBER ){
    TestPoint(0);
    return 1;
  }
  TestPoint(0);
  return 0;
}

/*
** Do as much layout as possible on the block of text defined by
** the HtmlLayoutContext.
*/
void HtmlLayoutBlock(HtmlLayoutContext *pLC){
  HtmlElement *p, *pNext;

  for(p=pLC->pStart; p && p!=pLC->pEnd; p=pNext){
    int lineWidth;
    int actualWidth;
    int y = 0;
    int leftMargin;
    int maxX = 0;

    /* Do as much break markup as we can. */
    while( p && p!=pLC->pEnd ){
      pNext = DoBreakMarkup(pLC, p);
      if( pNext==p ){ TestPoint(0); break; }
      if( pNext ){
        TRACE(HtmlTrace_BreakMarkup,
           ("Processed token %s as break markup\n", HtmlTokenName(p)));
        pLC->pStart = p;
      }
      p = pNext;
      TestPoint(0);
    }
    if( p==0 || p==pLC->pEnd ){ TestPoint(0); break; }

    /* We might try several times to layout a single line... */   
    while( 1 ){

      /* Compute margins */
      ComputeMargins(pLC, &leftMargin, &y, &lineWidth);

      /* Layout a single line of text */
      pNext = GetLine(pLC, p, pLC->pEnd, lineWidth, &actualWidth);
      TRACE(HtmlTrace_GetLine,
         ("GetLine page=%d left=%d right=%d available=%d used=%d\n",
         pLC->pageWidth, pLC->left, pLC->right, lineWidth, actualWidth));
      FixAnchors(p,pNext,pLC->bottom);

      /* Move down and repeat the layout if we exceeded the available
      ** line length and it is possible to increase the line length by
      ** moving past some obsticle.
      */
      if( actualWidth > lineWidth && InWrapAround(pLC) ){
        ClearObstacle(pLC, CLEAR_First);
        TestPoint(0);
        continue;
      }

      /* Lock the line into place and exit the loop */
      y = FixLine(p, pNext, y, lineWidth, actualWidth, leftMargin, &maxX);
      TestPoint(0);
      break;
    }

    /* If a line was completed, advance to the next line */
    if( pNext && actualWidth>0 && y > pLC->bottom ){
      pLC->bottom = y;
      pLC->headRoom = 0;
      pLC->pStart = pNext;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( y > pLC->maxY ){
      pLC->maxY = y;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( maxX > pLC->maxX ){
      pLC->maxX = maxX;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
}

/*
** Advance the layout as far as possible
*/
void HtmlLayout(HtmlWidget *htmlPtr){
  HtmlLayoutContext *pLC;
  int btm;

  if( htmlPtr->pFirst==0 ) return;
  HtmlLock(htmlPtr);
  HtmlSizer(htmlPtr);
  if( HtmlUnlock(htmlPtr) ) return;
  pLC = &htmlPtr->layoutContext;
  pLC->htmlPtr = htmlPtr;
  pLC->pageWidth = htmlPtr->realWidth - 2*(htmlPtr->inset + htmlPtr->padx);
  pLC->left = 0;
  pLC->right = 0;
  pLC->pStart = htmlPtr->nextPlaced;
  if( pLC->pStart==0 ){
    pLC->pStart = htmlPtr->pFirst;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  if( pLC->pStart ){
    pLC->maxX = htmlPtr->maxX;
    pLC->maxY = htmlPtr->maxY;
    btm = pLC->bottom;
    HtmlLayoutBlock(pLC);
    htmlPtr->maxX = pLC->maxX;
    htmlPtr->maxY = pLC->maxY;
    htmlPtr->nextPlaced = pLC->pStart;
    htmlPtr->flags |= HSCROLL | VSCROLL;
    HtmlRedrawText(htmlPtr, btm);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
}
