/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Interned script strings and reference counts.
===========================================================================
*/
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>
#include "flare/scr_stringlist.h"
#include "flare/scr_memorytree.h"

#define HASH_TABLE_SIZE 16384
#define MT_NODE_SIZE 8

enum { HASH_STAT_FREE, HASH_ENTRY_MOVABLE, HASH_ENTRY_HEAD };
enum { ERR_DROP = 1 };

typedef struct RefString {
    unsigned short refCount;
    unsigned char byteLen;
    unsigned char user;
    char str[1];
} RefString;


/* The hash head owns the first string; collisions form a circular chain.
   Free entries use the same links as a doubly linked free list. */
struct HashEntry {
    union {
        unsigned short status_next;
        struct {
            unsigned short next : 14;
            unsigned short status : 2;
        };
    };
    union {
        unsigned short prev;
        unsigned short str;
    } u;
};

#define HASH_STAT_MASK    0xc000
#define HASH_STAT_MOVABLE 0x4000
#define HASH_STAT_HEAD    0x8000

static HashEntry hashTable[HASH_TABLE_SIZE];
static HashEntry *lastHash;

/*
=============
GetRefString
=============
*/
static RefString *GetRefString(unsigned short stringValue)
{
    return (RefString *)(mt_buffer + stringValue * MT_NODE_SIZE);
}

/*
=============
GetRefString
=============
*/
static RefString *GetRefString(const char *str)
{
    return (RefString *)(str - offsetof(RefString, str));
}

extern "C" {
/*
=============
SL_ConvertToString
=============
*/
const char *SL_ConvertToString(unsigned short stringValue)
{
    if (stringValue) {
        return GetRefString(stringValue)->str;
    }
    return 0;
}

/*
=============
SL_ConvertFromString
=============
*/
unsigned int SL_ConvertFromString(const char *str)
{
    return ((char *)GetRefString(str) - mt_buffer) / MT_NODE_SIZE;
}
}

/*
=============
GetHashCode
=============
*/
static unsigned short GetHashCode(const char *str, unsigned int len)
{
    unsigned int hash;
    const char *text;
    if (len < 256) {
        text = str;
        hash = 0;
        while (len--) {
            hash = hash * 31 + *text++;
        }
        return hash % (HASH_TABLE_SIZE - 1) + 1;
    }
    hash = len >> 2;
    return hash % (HASH_TABLE_SIZE - 1) + 1;
}

extern "C" {
unsigned short SL_FindStringOfLen(const char *str, unsigned int len);
unsigned short SL_GetStringOfLen(const char *str, unsigned int user, unsigned int len, int type);
void SL_FreeString(unsigned short stringValue, const char *str, unsigned int len);
void Com_Error(int code, const char *format, ...);
void Com_Memcpy(void *dest, const void *src, unsigned int count);

/*
=============
SL_Init
=============
*/
void SL_Init(void)
{
    unsigned int i;
    unsigned short prev;
    HashEntry *entry;
    MT_Init();
    hashTable[0].status_next = 0;
    prev = 0;
    for (i = 1; i < HASH_TABLE_SIZE; i++) {
        entry = &hashTable[i];
        entry->status_next = 0;
        hashTable[prev].status_next |= i;
        entry->u.prev = prev;
        prev = i;
    }
    hashTable[0].u.prev = prev;
}

/*
=============
SL_FindStringOfLen
=============
*/
unsigned short SL_FindStringOfLen(const char *str, unsigned int len)
{
    unsigned short hash, index, prev, value;
    HashEntry *entry, *newEntry;
    RefString *refStr;
    hash = GetHashCode(str, len);
    entry = &hashTable[hash];
    if (entry->status == HASH_ENTRY_HEAD) {
        value = entry->u.str;
        refStr = GetRefString(value);
        if (refStr->byteLen == (unsigned char)len && !memcmp(refStr->str, str, len)) {
            return value;
        }

        prev = hash;
        index = entry->next;
        newEntry = &hashTable[index];
        while (newEntry != entry) {
            refStr = GetRefString(newEntry->u.str);
            if (refStr->byteLen == (unsigned char)len && !memcmp(refStr->str, str, len)) {
                hashTable[prev].next = newEntry->next;
                newEntry->next = entry->next;
                entry->status_next = (entry->status_next & HASH_STAT_MASK) | index;
                value = newEntry->u.str;
                newEntry->u.str = entry->u.str;
                entry->u.str = value;
                return value;
            }
            prev = index;
            index = newEntry->next;
            newEntry = &hashTable[index];
        }
    }
    return 0;
}



/*
=============
SL_FindString
=============
*/
unsigned short SL_FindString(const char *str)
{
    return SL_FindStringOfLen(str, strlen(str) + 1);
}

/*
=============
SL_FindLowercaseString
=============
*/
unsigned short SL_FindLowercaseString(const char *str)
{
    int len, i;
    char *lowercase;
    len = strlen(str) + 1;
    lowercase = (char *)_alloca(len);
    for (i = len - 1; i >= 0; i--) {
        lowercase[i] = tolower(str[i]);
    }
    return SL_FindStringOfLen(lowercase, len);
}

/*
=============
SL_GetStringOfLen
=============
*/
unsigned short SL_GetStringOfLen(const char *str, unsigned int user, unsigned int len, int type)
{
    unsigned short hash, index, prev, value;
    HashEntry *entry, *newEntry;
    RefString *refStr;
    unsigned short status;

    hash = GetHashCode(str, len);
    entry = &hashTable[hash];
    status = entry->status_next & HASH_STAT_MASK;
    if (status == HASH_STAT_HEAD) {
        refStr = GetRefString(entry->u.str);
        if (refStr->byteLen == (unsigned char)len && !memcmp(refStr->str, str, len)) {
            if (!(refStr->user & user)) {
                refStr->user |= user;
                refStr->refCount++;
            }
            return entry->u.str;
        }
        prev = hash;
        index = entry->next;
        newEntry = &hashTable[index];
        while (newEntry != entry) {
            refStr = GetRefString(newEntry->u.str);
            if (refStr->byteLen == (unsigned char)len && !memcmp(refStr->str, str, len)) {
                hashTable[prev].next = newEntry->next;
                newEntry->next = entry->next;
                entry->status_next = (entry->status_next & HASH_STAT_MASK) | index;
                value = newEntry->u.str;
                newEntry->u.str = entry->u.str;
                entry->u.str = value;
                if (!(refStr->user & user)) {
                    refStr->user |= user;
                    refStr->refCount++;
                }
                return entry->u.str;
            }
            prev = index;
            index = newEntry->next;
            newEntry = &hashTable[index];
        }
        {
            unsigned short freeIndex = hashTable[0].status_next;
            HashEntry *newEntry;
            if (!freeIndex) {
                Com_Error(ERR_DROP, "\x15" "exceeded maximum number of script strings\n");
            }
            newEntry = &hashTable[freeIndex];
            unsigned short newNext = newEntry->next;
            hashTable[0].status_next = newNext;
            hashTable[newNext].u.prev = 0;
            newEntry->status_next = entry->next | HASH_STAT_MOVABLE;
            entry->next = freeIndex;
            newEntry->u.str = entry->u.str;
        }
    } else {
        if (!status) {
            unsigned short next = entry->next;
            unsigned short prev = entry->u.prev;
            hashTable[prev].status_next = (hashTable[prev].status_next & HASH_STAT_MASK) | next;
            hashTable[next].u.prev = prev;
        } else {
            unsigned short next = entry->next;
            unsigned short prev = next;
            unsigned short freeIndex;
            HashEntry *newEntry;
            while (hashTable[prev].next != hash) {
                prev = hashTable[prev].next;
            }
            freeIndex = hashTable[0].status_next;
            if (!freeIndex) {
                Com_Error(ERR_DROP, "\x15" "exceeded maximum number of script strings\n");
            }
            newEntry = &hashTable[freeIndex];
            unsigned short newNext = newEntry->next;
            hashTable[0].status_next = newNext;
            hashTable[newNext].u.prev = 0;
            hashTable[prev].status_next = (hashTable[prev].status_next & HASH_STAT_MASK) | freeIndex;
            newEntry->status_next = next | HASH_STAT_MOVABLE;
            newEntry->u.str = entry->u.str;
        }
        entry->status_next = hash | HASH_STAT_HEAD;
    }
    value = MT_AllocIndex(len + offsetof(RefString, str), type);
    entry->u.str = value;
    refStr = GetRefString(value);
    Com_Memcpy(refStr->str, str, len);
    refStr->user = user;
    refStr->refCount = 0;
    refStr->byteLen = len;
    return entry->u.str;
}

/*
=============
SL_GetString_
=============
*/
unsigned short SL_GetString_(const char *str, unsigned int user, int type)
{
    return SL_GetStringOfLen(str, user, strlen(str) + 1, type);
}

/*
=============
SL_GetString
=============
*/
unsigned short SL_GetString(const char *str, unsigned int user)
{
    return SL_GetStringOfLen(str, user, strlen(str) + 1, 6);
}

/*
=============
SL_GetLowercaseStringOfLen
=============
*/
unsigned short SL_GetLowercaseStringOfLen(const char *str, unsigned int user, unsigned int len, int type)
{
    char *lowercase;
    int i;
    lowercase = (char *)_alloca(len);
    for (i = len - 1; i >= 0; i--) {
        lowercase[i] = tolower(str[i]);
    }
    return SL_GetStringOfLen(lowercase, user, len, type);
}

/*
=============
SL_GetLowercaseString_
=============
*/
unsigned short SL_GetLowercaseString_(const char *str, unsigned int user, int type)
{
    return SL_GetLowercaseStringOfLen(str, user, strlen(str) + 1, type);
}

/*
=============
SL_GetLowercaseString
=============
*/
unsigned short SL_GetLowercaseString(const char *str, unsigned int user)
{
    return SL_GetLowercaseStringOfLen(str, user, strlen(str) + 1, 6);
}

/*
=============
SL_TransferRefToString
=============
*/
void SL_TransferRefToString(unsigned short stringValue, unsigned char user)
{
    RefString *refStr;
    refStr = GetRefString(stringValue);
    if (refStr->user & user) {
        refStr->refCount--;
    } else {
        refStr->user |= user;
    }
}

/*
=============
SL_AddRefToString
=============
*/
void SL_AddRefToString(unsigned short stringValue)
{
    RefString *refStr;
    refStr = GetRefString(stringValue);
    refStr->refCount++;
}

/*
=============
SL_FreeString
=============
*/
void SL_FreeString(unsigned short stringValue, const char *str, unsigned int len)
{
    unsigned short hash, index, prev;
    HashEntry *entry, *newEntry;
    unsigned short newNext;
    hash = GetHashCode(str, len);
    entry = &hashTable[hash];
    MT_FreeIndex(stringValue, len + offsetof(RefString, str));
    index = entry->next;
    newEntry = &hashTable[index];
    if (entry->u.str == stringValue) {
        if (newEntry != entry) {
            lastHash = entry;
            entry->next = newEntry->next;
            entry->status = HASH_ENTRY_HEAD;
            entry->u.str = newEntry->u.str;
        } else {
            newEntry = entry;
            index = hash;
        }
    } else {
        prev = hash;
        while (newEntry->u.str != stringValue) {
            prev = index;
            index = newEntry->next;
            newEntry = &hashTable[index];
        }
        hashTable[prev].next = newEntry->next;
    }
    newNext = hashTable[0].status_next;
    newEntry->status_next = newNext;
    newEntry->u.prev = 0;
    hashTable[newNext].u.prev = index;
    hashTable[0].status_next = index;
}

/*
=============
SL_RemoveRefToString
=============
*/
void SL_RemoveRefToString(unsigned short stringValue)
{
    RefString *refStr;
    refStr = GetRefString(stringValue);
    if (refStr->refCount) {
        refStr->refCount--;
    } else {
        SL_FreeString(stringValue, refStr->str, strlen(refStr->str) + 1);
    }
}

/*
=============
SL_RemoveRefToStringOfLen
=============
*/
void SL_RemoveRefToStringOfLen(unsigned short stringValue, unsigned int len)
{
    RefString *refStr;
    refStr = GetRefString(stringValue);
    if (refStr->refCount) {
        refStr->refCount--;
    } else {
        SL_FreeString(stringValue, refStr->str, len);
    }
}

/*
=============
Scr_SetString
=============
*/
void Scr_SetString(unsigned short *to, unsigned short from)
{
    if (*to) {
        SL_RemoveRefToString(*to);
    }
    if (from) {
        SL_AddRefToString(from);
    }
    *to = from;
}

/*
=============
Scr_AllocString
=============
*/
unsigned short Scr_AllocString(const char *str)
{
    return SL_GetString_(str, 1, 6);
}

/*
=============
SL_GetStringForFloat
=============
*/
unsigned short SL_GetStringForFloat(float value)
{
    char str[128];
    sprintf(str, "%g", value);
    return SL_GetString_(str, 0, 14);
}

/*
=============
SL_GetStringForInt
=============
*/
unsigned short SL_GetStringForInt(int value)
{
    char str[128];
    sprintf(str, "%i", value);
    return SL_GetString_(str, 0, 14);
}

/*
=============
SL_GetStringForVector
=============
*/
unsigned short SL_GetStringForVector(const float *value)
{
    char str[128];
    sprintf(str, "(%.2f, %.2f, %.2f)", value[0], value[1], value[2]);
    return SL_GetString_(str, 0, 14);
}

/*
=============
SL_ShutdownSystem
=============
*/
void SL_ShutdownSystem(unsigned char user)
{
    unsigned int i;
    HashEntry *entry;
    RefString *refStr;

    for (i = 1; i < HASH_TABLE_SIZE; i++) {
        do {
            entry = &hashTable[i];
            if (entry->status == HASH_STAT_FREE) {
                break;
            }
            refStr = GetRefString(entry->u.str);
            if (!(refStr->user & user)) {
                break;
            }
            refStr->user &= ~user;
            lastHash = 0;
            SL_RemoveRefToString(entry->u.str);
        } while (lastHash);
    }
}

/*
=============
CreateCanonicalFilename
=============
*/
static void CreateCanonicalFilename(char *out, const char *in, int count)
{
    char c;
    do {
        do {
            c = *in++;
        } while (c == '\\' || c == '/');
        while ((unsigned char)c >= 32) {
            *out++ = tolower(c);
            if (!--count) {
                Com_Error(ERR_DROP, "\x15" "Filename '%s' exceeds maximum length of %d", in, count);
            }
            if (c == '\\') {
                break;
            }
            c = *in++;
            if (c == '/') {
                c = '\\';
            }
        }
    } while (c);
    *out = 0;
}

/*
=============
Scr_CreateCanonicalFilename
=============
*/
unsigned short Scr_CreateCanonicalFilename(const char *filename)
{
    char canonical[1024];
    CreateCanonicalFilename(canonical, filename, sizeof(canonical));
    return SL_GetString_(canonical, 0, 7);
}
}
