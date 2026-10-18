/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Ordered occluder-pair registry.
===========================================================================
*/

#include "flare/occluderpairs.h"


/*
=============
FindOrCreateOccluderPair

Reuse an ordered pair or prepend a new entry to the registry.
=============
*/
OccluderPair *FindOrCreateOccluderPair(Occluder *first, Occluder *second)
{
    OccluderPair *pair;
    for (pair = occluderPairs; pair; pair = pair->next)
    {
        if (pair->first == first && pair->second == second)
            return pair;
    }

    numOccluderPairs++;
    pair = new OccluderPair;
    pair->second = second;
    pair->first = first;
    pair->next = occluderPairs;
    occluderPairs = pair;
    return pair;
}
