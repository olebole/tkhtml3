static char const rcsid[] = "@(#) $Id: htmlindex.c,v 1.10 2002/03/06 18:10:58 peter Exp $";
/*
** Routines that deal with indexes
**
** Copyright (C) 1997-2000 D. Richard Hipp
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
#include <ctype.h>
#include <string.h>
#include <tk.h>
#include "htmlindex.h"

/*
** Return a pointer to the Nth HtmlElement in the list.  If there
** is no Nth element, return 0 if flag==0 and return either the first
** or last element (whichever is closest) if flag!=0
*/
HtmlElement *HtmlTokenByIndex(HtmlWidget *htmlPtr, int N, int flag){
  HtmlElement *p;
  int n;

  if (!N) 
    return htmlPtr->pFirst;
  if( N > htmlPtr->nToken/2 ){
    /* Start at the end and work back toward the beginning */
    for(p=htmlPtr->pLast, n=htmlPtr->nToken; p; p=p->base.pPrev){
      if( p->base.type!=Html_Block ){
        if( p->base.id==N ){ break; }
        n--;
      }
    }
  }else{
    /* Start at the beginning and work forward */
    for(p=htmlPtr->pFirst; p; p = p->base.pNext){
      if( p->base.type!=Html_Block ){
/*        N--; */
        if( N==p->base.id ){ break; }
      }
    }
  }
  return p;
}

/*
** Return the token number for the given HtmlElement
*/
int HtmlTokenNumber(HtmlElement *p){
  int n = 0;

  if (!p) return -1;
  return p->base.id;

  while( p ){
    if( p->base.type!=Html_Block ){ 
      n++;
    }
    p = p->base.pPrev;
  }
  return n;
}

/*
** Find the maximum index for the given token
*/
static void maxIndex(HtmlElement *p, int *pIndex, int islast){
  if( p==0 ){
    *pIndex = 0;
  }else{
    switch( p->base.type ){
      case Html_Text:
        *pIndex = p->base.count-islast;
        break;
      case Html_Space:
        if( p->base.style.flags & STY_Preformatted ){
          *pIndex = p->base.count-islast;
        }else{
          *pIndex = 0;
        }
        break;
      default:
        *pIndex = 0;
        break;
    }
  }
}

#ifndef _TCLHTML_
/*
** Given a Block and an x coordinate, find the Index of the character
** that is closest to the given x coordinate.
**
** The x-coordinate might specify a point to the left of the block,
** in which case the procedure returns the first token and a character
** index of 0.  Or the x-coordinate might specify a point to the right
** of the block, in which case the last token is returned with an index
** equal to its last character.
*/
static void FindIndexInBlock(
  HtmlWidget *htmlPtr,       /* The widget */
  HtmlBlock *pBlock,         /* The block */
  int x,                     /* The x coordinate */
  HtmlElement **ppToken,     /* Write the closest token here */
  int *pIndex                /* Write the charater index in ppToken here */
){
  HtmlElement *p;
  Tk_Font font;
  int len;
  int n;

  p = pBlock->base.pNext;
  HtmlLock(htmlPtr);
  font = HtmlGetFont(htmlPtr, p->base.style.font);
  if( HtmlUnlock(htmlPtr) ){
    *ppToken = p;
    *pIndex = 0;
    return;
  }
  if( x <= pBlock->left ){
    *ppToken = p;
    *pIndex = 0;
    return;
  }else if( x>= pBlock->right ){
    *ppToken = p;
    *pIndex = 0;
    while( p && p->base.type!=Html_Block ){
      *ppToken = p;
      p = p->base.pNext;
    }
    p = *ppToken;
    if( p && p->base.type==Html_Text ){
      *pIndex = p->base.count - 1;
    }
    return;
  }
  if( pBlock->n==0 ){
    *ppToken = p;
    *pIndex = 0;
  }
  n = Tk_MeasureChars(font, pBlock->z, pBlock->n, x - pBlock->left, 0, &len);
  *pIndex = 0;
  *ppToken = 0;
  while( p && n>=0 ){
    switch( p->base.type ){
      case Html_Text:
        if( n<p->base.count ){
          *pIndex = n;
        }else{
          *pIndex = p->base.count - 1;
        }
        *ppToken = p;
        n -= p->base.count;
        break;
      case Html_Space:
        if( p->base.style.flags & STY_Preformatted ){
          if( n<p->base.count ){
            *pIndex = n;
          }else{
            *pIndex = p->base.count - 1;
          }
          *ppToken = p;
          n -= p->base.count;
        }else{
          *pIndex = 0;
          *ppToken = p;
          n--;
        }
        break;
      default:
        break;
    }
    if( p ){
      p = p->base.pNext;
    }else{
    }
  }
}
#endif /* _TCLHTML_ */

/*
** Convert an Element-based index into a Block-based index.
**
** In other words, given a pointer to an element and an index
** of a particular character within that element, compute a
** pointer to the HtmlBlock used to display that character and
** the index in the HtmlBlock of the character.
*/
void HtmlIndexToBlockIndex(
  HtmlWidget *htmlPtr,        /* The widget */
  HtmlIndex sIndex,           /* The index to be translated */
  HtmlBlock **ppBlock,        /* Write the corresponding block here */
  int *piIndex                /* Write the block index here */
){
  int n = sIndex.i;
  HtmlElement *p,*lp;

  if( sIndex.p==0 ){
    *ppBlock = 0;
    *piIndex = 0;
    return;
  }
  p = sIndex.p->base.pPrev;
  while( p && p->base.type!=Html_Block ){
    switch( p->base.type ){
      case Html_Text:
        n += p->base.count;
        break;
      case Html_Space:
        if( p->base.style.flags & STY_Preformatted ){
          n += p->base.count;
        }else{
          n++;
        }
        break;
      default:
        break;
    }
    lp=p;
    p = p->base.pPrev;
  }
  if( p ){
    *ppBlock = &p->block;
    *piIndex = n;
    return;
  }
  for(p=sIndex.p; p && p->base.type!=Html_Block; p=p->base.pNext){
  }
  *ppBlock = &p->block;
  *piIndex = 0;
}


/* Modify an index for both pointer and char +/-/=N */
int HtmlIndexMod( HtmlWidget *htmlPtr,  HtmlElement** pp, int *ip, char *cp) {
  char nbuf[50];
  int i, x, cnt, ccnt[2], cflag[2];
  if (pp==0 || !*pp) return -1;
  ccnt[0]=ccnt[1]=cflag[0]=cflag[1]=0;
  x=0;
  while (*cp && x<2) {
    cnt=0;
    i=1;
    while (i<45 && isdigit(cp[i])) nbuf[i-1]=cp[i++];
    if (i>1) {
      nbuf[i-1]=0;
      cnt=atoi(nbuf);
      if (cnt<0) return -1;
    }
    switch (*cp) {
      case '+': if (i==1) ccnt[x]=1; else ccnt[x]=cnt; break;
      case '-': if (i==1) ccnt[x]=-1; else ccnt[x]=-cnt; break;
      case '=': ccnt[x]=0; cflag[x]=1; break;
      default: return -1;
    }
    cp+=i;
    x++;
  }
  if (ccnt[0]>0) {
    for (i=0; i<ccnt[0] && (*pp)->pNext; i++) {
      *pp=(*pp)->pNext;
      while ((*pp)->base.type==Html_Block && (*pp)->pNext) 
        *pp=(*pp)->pNext;
    }
  } else if (ccnt[0]<0) {
    for (i=0; ccnt[0]<i && (*pp)->base.pPrev; i--) {
      printf("i=%d,cnt=%d\n",i,ccnt[0]);
      *pp=(*pp)->base.pPrev;
      while ((*pp)->base.type==Html_Block && (*pp)->base.pPrev) 
        *pp=(*pp)->base.pPrev;
    }
  }
  if (ccnt[1]>0)
    for (i=0; i<ccnt[1]; i++) (*ip)++;
  else if (ccnt[1]<0)
    for (i=0; i>ccnt[1]; i--) (*ip)--;
  return 0;
}

/*
** Given a base index name (without any modifiers) return a pointer
** to the token described, and the character within that token.
**
** Valid input forms include:
**
**       N.M          Token number N (with numbering starting at 1) and
**                    character number M (with numbering starting at 0).
**
**       M.X	      Like above, but token is markup and X is an attribute.
**
**       begin        The start of all text
**
**       end          The end of all text
**
**       N.last       Last character of token number N.
**
**       N.end        One past last character of token number N.
**
**       sel.first    First character of the selection.
**
**       sel.last     Last character of the selection.
**
**       sel.end      On past last character of the selection.
**
**       insert       The character holding the insertion cursor.
**
**       @X,Y         The character a location X,Y of the clipping window.
**
**       &DOM         The DOM Address of a token.
**
** Zero is returned if we are successful and non-zero if there is
** any kind of error.
**
** If the given token doesn't exist (for example if there are only 10
** tokens and 11.5 is requested) then *ppToken is left pointing to NULL.
** But the function still returns 0 for success.
*/
static int DecodeBaseIndex(
  HtmlWidget *htmlPtr,     /* The HTML widget we are dealing with */
  const char *Baseind,     /* The base index string */
  HtmlElement **ppToken,   /* Write the pointer to the token here */
  int *pIndex              /* Write the character offset here */
){
  int x, y;
  int n;
  int i;
  HtmlElement *p;
  HtmlBlock *pBlock;
  HtmlBlock *pNearby;
  int dist = 1000000;
  int rc = 0;
  char buf[200], *zBase=buf, *suffix, *ep;
  strncpy(buf,Baseind,sizeof(buf));
  buf[sizeof(buf)-1]=0;
  while( isspace(*zBase) ){ zBase++; }
  ep=zBase;
  while (*ep && !isspace(*ep)) ep++;
  *ep=0;
  if ((suffix=strchr(zBase,':'))) *suffix=0;

  switch( *zBase ){
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '0':
      n = sscanf(zBase,"%d.%d",&x,&y);
      if( n>0 ){
        p = *ppToken = HtmlTokenByIndex(htmlPtr, x, 0);
      }
      if( n==2 ){
        *pIndex = y;
      }else{
        for(i=1; isdigit(zBase[i]); i++){ }
        if( zBase[i]==0 ){
          *pIndex = 0;
        }else if( strcmp(&zBase[i],".last")==0 ){
          maxIndex(p,pIndex,1);
        }else if( strcmp(&zBase[i],".end")==0 ){
          maxIndex(p,pIndex,0);
          (*pIndex)++;
        }else{
          if (n==1 && p && HtmlIsMarkup(p) && zBase[i]=='.' && 
	    HtmlMarkupArg(p,zBase+i+1,0)) *pIndex = 0;
	  else
            rc = 1;
        }
      }
      break;
    case 'b':
      if( strcmp(zBase,"begin")==0 ){
        p = *ppToken = htmlPtr->pFirst;
        *pIndex=0;
      }else{
        rc = 1;
      }
      break;
    case 'e':
      if( strcmp(zBase,"end")==0 ){
        p = *ppToken = htmlPtr->pLast;
        maxIndex(p,pIndex,0);
      }else{
        rc = 1;
      }
      break;
    case 'l':
      if( strcmp(zBase,"last")==0 ){
        p = *ppToken = htmlPtr->pLast;
        maxIndex(p,pIndex,1);
      }else{
        rc = 1;
      }
      break;
    case 's':
      if( strcmp(zBase,"sel.first")==0 ){
        *ppToken = htmlPtr->selBegin.p;
        *pIndex = htmlPtr->selBegin.i;
      }else if( strcmp(zBase,"sel.last")==0 ){
        *ppToken = htmlPtr->selEnd.p;
        *pIndex = htmlPtr->selEnd.i;
      }else if( strcmp(zBase,"sel.end")==0 ){
        *ppToken = htmlPtr->selEnd.p;
        *pIndex = htmlPtr->selEnd.i+1;
      }else{
        rc = 1;
      }
      break;
    case 'i':
      if( strcmp(zBase,"insert")==0 ){
        *ppToken = htmlPtr->ins.p;
        *pIndex = htmlPtr->ins.i;
      }else{
        rc = 1;
      }
      break;
    case '&':
      *pIndex=0;
      if (HtmlDomIdLookup(htmlPtr, "id", zBase+1, ppToken))
	rc=1;
      break;
    case '@':
#ifdef _TCLHTML_
      rc=1;
#else
      n = sscanf(zBase,"@%d,%d",&x,&y);
      if( n!=2 ){
        rc = 1;
        break;
      }
      x += htmlPtr->xOffset;
      y += htmlPtr->yOffset;
      pNearby = 0;
      *ppToken = htmlPtr->pLast;
      *pIndex = 0;
      for(pBlock=htmlPtr->firstBlock; pBlock; pBlock=pBlock->pNext){
        int dotest;
        if( pBlock->n==0 ){
          switch( pBlock->base.pNext->base.type ){
            case Html_LI:
            case Html_IMG:
            case Html_INPUT:
            case Html_TEXTAREA:
            case Html_SELECT:
              dotest = 1;
              break;
            default:
              dotest = 0;
              break;
          }
        }else{
          dotest = 1;
        }
        if (dotest){ 
          if( pBlock->top <= y && pBlock->bottom >= y ){
            if( pBlock->left > x ){
              if( pBlock->left - x < dist ){
                dist = pBlock->left - x;
                pNearby = pBlock;
              }
            }else if( pBlock->right < x ){
              if( x - pBlock->right < dist ){
                dist = x - pBlock->right;
                pNearby = pBlock;
              }
            }else{
              HtmlLock(htmlPtr);
              FindIndexInBlock(htmlPtr, pBlock, x, ppToken, pIndex);
              if( HtmlUnlock(htmlPtr) ) return 1;
              break;
            }
          }else{
            int distY;
            int distX;

            if( pBlock->bottom < y ){
              distY = y - pBlock->bottom;
            }else{
              distY = pBlock->top - y;
            }
            if( pBlock->left > x ){
              distX = pBlock->left - x;
            }else if( pBlock->right < x ){
              distX = x - pBlock->right;
            }else{
              distX = 0;
            }
            if( distX + 4*distY < dist ){
              dist = distX + 4*distY;
              pNearby = pBlock;
            }
          }
        }
      }
      if( pBlock==0 ){
        if( pNearby ){
          HtmlLock(htmlPtr);
          FindIndexInBlock(htmlPtr, pNearby, x, ppToken, pIndex);
          if( HtmlUnlock(htmlPtr) ) return 1;
        }
      }
      break;
#endif /* _TCLHTML_ */
    default:
      rc = 1;
      break;
  }
  if (suffix) HtmlIndexMod(htmlPtr,ppToken,pIndex,suffix+1);
  return rc;
}

/*
** This routine decodes a complete index specification.  A complete
** index consists of the base specification followed by modifiers.
*/
int HtmlGetIndex(
  HtmlWidget *htmlPtr,     /* The widget */
  char *zIndex,            /* Complete text of the index spec */
  HtmlElement **ppToken,   /* Write the pointer to the token here */
  int *pIndex              /* Write the character offset here */
){
  return DecodeBaseIndex(htmlPtr, zIndex, ppToken, pIndex);
}
