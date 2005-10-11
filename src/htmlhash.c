/*
 * htmlhash.c --
 *
 *     This file contains code to extend the Tcl hash mechanism to use
 *     case-insensitive strings as hash-keys. This code was copied from the
 *     Tcl core code for regular string hashes and modified only slightly.
 *
 *----------------------------------------------------------------------------
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
 * COPYRIGHT:
 */

#include <tcl.h>
#include <strings.h>
#include <ctype.h>
#include "html.h"

/*
 *---------------------------------------------------------------------------
 *
 * compareKey --
 *
 *     The compare function for the case-insensitive string hash. Compare a
 *     new key to the key of an existing hash-entry.
 *
 * Results:
 *     True if the two keys are the same, false if not.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
compareKey(keyPtr, hPtr)
    VOID *keyPtr;               /* New key to compare. */
    Tcl_HashEntry *hPtr;        /* Existing key to compare. */
{   
    register CONST char *p1 = (CONST char *) keyPtr;
    register CONST char *p2 = (CONST char *) hPtr->key.string;

    return !stricmp(p1, p2);
}

/*
 *---------------------------------------------------------------------------
 *
 * hashKey --
 *
 *     Generate a 4-byte hash of the NULL-terminated string pointed to by 
 *     keyPtr. The hash is case-insensitive.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static unsigned int 
hashKey(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key from which to compute hash value. */
{
    register CONST char *string = (CONST char *) keyPtr;
    register unsigned int result;
    register int c;

    result = 0;

    for (c=*string++ ; c ; c=*string++) {
        result += (result<<3) + tolower(c);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * allocEntry --
 *
 *     Allocate enough space for a Tcl_HashEntry and associated string key.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_HashEntry * 
allocEntry(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;    /* Hash table. */
    VOID *keyPtr;               /* Key to store in the hash table entry. */
{
    CONST char *string = (CONST char *) keyPtr;
    Tcl_HashEntry *hPtr;
    unsigned int size;

    size = sizeof(Tcl_HashEntry) + strlen(string) + 1 - sizeof(hPtr->key);
    if (size < sizeof(Tcl_HashEntry)) {
        size = sizeof(Tcl_HashEntry);
    }
    hPtr = (Tcl_HashEntry *) ckalloc(size);
    strcpy(hPtr->key.string, string);

    return hPtr;
}

/*
 * Hash key type for case-insensitive hash.
 */
static Tcl_HashKeyType hash_key_type = {
    0,                                  /* version */
    0,                                  /* flags */
    hashKey,                            /* hashKeyProc */
    compareKey,                         /* compareKeysProc */
    allocEntry,                         /* allocEntryProc */
    NULL                                /* freeEntryProc */
};

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCaseInsenstiveHashType --
 *
 *     Return a pointer to the hash key type for case-insensitive string
 *     hashes. This can be used to initialize a hash table as follows:
 *
 *         Tcl_HashTable hash;
 *         Tcl_HashKeyType *pCase = HtmlCaseInsenstiveHashType();
 *         Tcl_InitCustomHashTable(&hash, TCL_CUSTOM_TYPE_KEYS, pCase);
 *
 * Results:
 *     Pointer to hash_key_type (see above).
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_HashKeyType *
HtmlCaseInsenstiveHashType() 
{
    return &hash_key_type;
}
