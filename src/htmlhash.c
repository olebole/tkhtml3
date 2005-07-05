/*
 * htmlhash.c --
 *
 *     This file contains code to extend the Tcl hash mechanism to use
 *     case-insensitive strings as hash-keys. This code was copied from the
 *     Tcl core code for regular string hashes and modified only slightly.
 *
 *--------------------------------------------------------------------------
 * COPYRIGHT:
 */

#include <tcl.h>

/*
 *---------------------------------------------------------------------------
 *
 * compareKey --
 *
 *     Compare a new key to the key of an existing hash-entry.
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

    return !strcasecmp(p1, p2);
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
