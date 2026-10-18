/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Size-class tree allocator for script memory.
===========================================================================
*/
#include <string.h>
#include "code/qcommon/qcommon.h"
#include "flare/scr_memorytree.h"

/* Size classes count powers of two eight-byte nodes. */
#define MEMORY_NODE_COUNT   65536
#define MT_NODE_SIZE        8
#define MEMORY_NODE_BITS    16
#define MT_BIT_TABLE_SIZE   256
#define MT_MAX_ALLOC_SIZE   65536

enum { ERR_DROP = 1 };

extern "C" {

/* Each free bucket stores its two tree links in the first node. */
struct MemoryNode {
    unsigned short prev;
    unsigned short next;
    unsigned int data;
};

static unsigned char *mt_size = 0;
static int mt_allocCount = 0;
static unsigned char mt_bitCount[MT_BIT_TABLE_SIZE] = {0};
static unsigned short mt_head[MEMORY_NODE_BITS + 1] = {0};
static unsigned char *mt_numBits = 0;
char *mt_buffer = 0;
static unsigned char mt_leftBits[MT_BIT_TABLE_SIZE] = {0};
static int mt_allocBuckets = 0;
static MemoryNode nodes[MEMORY_NODE_COUNT] = {0};
static unsigned char mt_logBits[MT_BIT_TABLE_SIZE] = {0};
int scrStringInited = 0;

/*
=============
MT_GetSubTreeSize
=============
*/
static int MT_GetSubTreeSize(int nodeNum)
{
    if (!nodeNum) {
        return 0;
    }
    return MT_GetSubTreeSize(nodes[nodeNum].prev) +
        MT_GetSubTreeSize(nodes[nodeNum].next) + 1;
}

/*
=============
MT_DumpTree
=============
*/
void MT_DumpTree(void)
{
    int i, count;
    Com_Printf("********************************\n");
    for (i = 0; i <= MEMORY_NODE_BITS; i++) {
        count = MT_GetSubTreeSize(mt_head[i]);
        Com_Printf("%d subtree has %d * %d = %d free buckets\n", i, count, 1 << i, count * (1 << i));
    }
    Com_Printf("********************************\n");
    Com_Printf("********************************\n");
    Com_Printf("total memory alloc buckets: %d (%d instances)\n", mt_allocBuckets, mt_allocCount);
    Com_Printf("total memory free buckets: %d\n", (MEMORY_NODE_COUNT - 1) - mt_allocBuckets);
    Com_Printf("********************************\n");
}

/*
=============
Scr_GetStringUsage
=============
*/
int Scr_GetStringUsage(void)
{
    return mt_allocBuckets;
}

/*
=============
MT_InitBits
=============
*/
static void MT_InitBits(void)
{
    int i, n, count;
    for (i = 0; i < MT_BIT_TABLE_SIZE; i++) {
        count = 0;
        for (n = i; n; n >>= 1) {
            if (n & 1) {
                count++;
            }
        }
        mt_bitCount[i] = count;

        /* Zero has eight trailing zero bits in the byte table. */
        count = 8;
        if (i & 255) {
            while (i & ((1 << --count) - 1)) {
            }
        }
        mt_leftBits[i] = count;

        count = 0;
        for (n = i; n; n >>= 1) {
            count++;
        }
        mt_logBits[i] = count;
    }
}

/*
=============
MT_GetScore

Rank free nodes by their alignment and position in the pool.
=============
*/
static int MT_GetScore(int nodeNum)
{
    int num, bits, score, hi, lo;
    num = MEMORY_NODE_COUNT - nodeNum;
    lo = (unsigned char)num;
    hi = (num >> 8) & 255;
    score = num - mt_bitCount[hi] - mt_bitCount[lo];
    bits = mt_leftBits[lo];
    if (!lo) {
        bits += mt_leftBits[hi];
    }
    return (1 << bits) + score;
}

/*
=============
MT_AddMemoryNode
=============
*/
static void MT_AddMemoryNode(int size, int newNode)
{
    unsigned short *parentNode;
    int node, newScore, range, nodeNum;
    parentNode = &mt_head[size];
    node = *parentNode;
    if (node) {
        newScore = MT_GetScore(newNode);
        range = MEMORY_NODE_COUNT;
        nodeNum = 0;
        do {
            if (MT_GetScore(node) < newScore) {
                *parentNode = newNode;
                nodes[newNode] = nodes[node];
                while (node) {
                    range >>= 1;
                    if (node < nodeNum) {
                        parentNode = &nodes[newNode].prev;
                        nodeNum -= range;
                    } else {
                        parentNode = &nodes[newNode].next;
                        nodeNum += range;
                    }
                    newNode = node;
                    node = *parentNode;
                    *parentNode = newNode;
                    nodes[newNode] = nodes[node];
                }
                return;
            }
            range >>= 1;
            if (newNode < nodeNum) {
                parentNode = &nodes[node].prev;
                nodeNum -= range;
            } else {
                parentNode = &nodes[node].next;
                nodeNum += range;
            }
            node = *parentNode;
        } while (node);
    }
    *parentNode = newNode;
    nodes[newNode].prev = 0;
    nodes[newNode].next = 0;
    return;
}

/*
=============
MT_RemoveMemoryNode
=============
*/
static bool MT_RemoveMemoryNode(int nodeNum, int size)
{
    unsigned short *parentNode;
    int oldNode, range, midpoint;
    MemoryNode node, old;
    parentNode = &mt_head[size];
    oldNode = *parentNode;
    range = MEMORY_NODE_COUNT;
    midpoint = 0;
    for (;;) {
        if (!oldNode) {
            return false;
        }
        if (nodeNum == oldNode) {
            break;
        }
        if (nodeNum == midpoint) {
            return false;
        }
        range >>= 1;
        if (nodeNum < midpoint) {
            parentNode = &nodes[oldNode].prev;
            midpoint -= range;
        } else {
            parentNode = &nodes[oldNode].next;
            midpoint += range;
        }
        oldNode = *parentNode;
    }
    node = nodes[nodeNum];
    for (;;) {
        if (!node.prev) {
            oldNode = node.next;
            *parentNode = oldNode;
            if (!oldNode) {
                return true;
            }
            parentNode = &nodes[oldNode].next;
        } else if (!node.next) {
            oldNode = node.prev;
            *parentNode = oldNode;
            parentNode = &nodes[oldNode].prev;
        } else {
            int prevScore = MT_GetScore(node.prev);
            int nextScore = MT_GetScore(node.next);
            if (prevScore < nextScore) {
                oldNode = node.next;
                *parentNode = oldNode;
                parentNode = &nodes[oldNode].next;
            } else {
                oldNode = node.prev;
                *parentNode = oldNode;
                parentNode = &nodes[oldNode].prev;
            }
        }
        old = node;
        node = nodes[oldNode];
        nodes[oldNode] = old;
    }
}

/*
=============
MT_RemoveHeadMemoryNode
=============
*/
static void MT_RemoveHeadMemoryNode(int size)
{
    unsigned short *parentNode;
    int oldNode;
    MemoryNode node, old;
    parentNode = &mt_head[size];
    node = nodes[*parentNode];
    for (;;) {
        if (!node.prev) {
            oldNode = node.next;
            *parentNode = oldNode;
            if (!oldNode) {
                return;
            }
            parentNode = &nodes[oldNode].next;
        } else if (!node.next) {
            oldNode = node.prev;
            *parentNode = oldNode;
            parentNode = &nodes[oldNode].prev;
        } else {
            int prevScore = MT_GetScore(node.prev);
            int nextScore = MT_GetScore(node.next);
            if (prevScore < nextScore) {
                oldNode = node.next;
                *parentNode = oldNode;
                parentNode = &nodes[oldNode].next;
            } else {
                oldNode = node.prev;
                *parentNode = oldNode;
                parentNode = &nodes[oldNode].prev;
            }
        }
        old = node;
        node = nodes[oldNode];
        nodes[oldNode] = old;
    }
}

/*
=============
MT_Init
=============
*/
void MT_Init(void)
{
    int i;
    mt_buffer = (char *)nodes;
    MT_InitBits();
    memset(mt_head, 0, sizeof(mt_head));
    nodes[0].prev = 0;
    nodes[0].next = 0;
    mt_numBits = mt_logBits;
    mt_size = mt_logBits;
    scrStringInited = 0;
    for (i = 0; i < MEMORY_NODE_BITS; i++) {
        MT_AddMemoryNode(i, 1 << i);
    }
    mt_allocCount = 0;
    mt_allocBuckets = 0;
}

/*
=============
MT_Error
=============
*/
static void MT_Error(const char *function, int numBytes)
{
    Com_Error(ERR_DROP, "\x15%s: failed allocation of %d bytes for script usage", function, numBytes);
}

/*
=============
MT_GetSize
=============
*/
static int MT_GetSize(int numBytes)
{
    int size;
    if (numBytes < MT_MAX_ALLOC_SIZE) {
        size = (numBytes + MT_NODE_SIZE - 1) / MT_NODE_SIZE - 1;
        if (size <= 255)
            return mt_logBits[size];
        return mt_logBits[size >> 8] + 8;
    }
    MT_Error("MT_GetSize: max allocation exceeded", numBytes);
    return 0;
}

/*
=============
MT_AllocIndex
=============
*/
unsigned short MT_AllocIndex(int numBytes, int type)
{
    int size, freeSize, nodeNum;
    size = MT_GetSize(numBytes);
    for (freeSize = size; freeSize <= MEMORY_NODE_BITS; freeSize++) {
        nodeNum = mt_head[freeSize];
        if (nodeNum) {
            MT_RemoveHeadMemoryNode(freeSize);
            while (freeSize != size) {
                freeSize--;
                MT_AddMemoryNode(freeSize, nodeNum + (1 << freeSize));
            }
            mt_allocCount++;
            mt_allocBuckets += 1 << size;
            return nodeNum;
        }
    }
    MT_Error("MT_AllocIndex", numBytes);
    return 0;
}

/*
=============
MT_FreeIndex
=============
*/
void MT_FreeIndex(unsigned short nodeNum, int numBytes)
{
    int size, lowBit;
    size = MT_GetSize(numBytes);
    mt_allocCount--;
    lowBit = 1 << size;
    mt_allocBuckets -= lowBit;
    while (size != MEMORY_NODE_BITS && MT_RemoveMemoryNode(nodeNum ^ lowBit, size)) {
        nodeNum &= ~lowBit;
        size++;
        lowBit = 1 << size;
    }
    MT_AddMemoryNode(size, nodeNum);
}

/*
=============
MT_Alloc
=============
*/
void *MT_Alloc(int numBytes, int type)
{
    return nodes + MT_AllocIndex(numBytes, type);
}

/*
=============
MT_Free
=============
*/
void MT_Free(void *p, int numBytes)
{
    MT_FreeIndex((MemoryNode *)p - nodes, numBytes);
}

/*
=============
MT_Realloc
=============
*/
int MT_Realloc(int oldSize, int newSize)
{
    return MT_GetSize(oldSize) == MT_GetSize(newSize);
}
}
