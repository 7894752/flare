/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Skeletal object allocation, animation and collision queries.
===========================================================================
*/
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "xmodel.h"
#include "flare/scr_stringlist.h"
#include "code/qcommon/qcommon.h"
#include "flare/xmodel.h"

/* Vector operations from the original Q3 q_shared.h. */
#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define	VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)

static unsigned short g_empty;


/* Duplicate-part data starts with a 128-bit mask and ends with a zero byte. */
/*
================
DObjInit

Intern the empty duplicate-bone record shared by skeletal objects.
The record contains a clear bone mask and a terminating zero byte.
================
*/
void DObjInit(void)
{
    int empty[5];
    Com_Memset(empty, 0, sizeof(empty));
    g_empty = SL_GetStringOfLen((const char *)empty, 0, 17, 12);
}

/*
================
DObjShutdown

Release the shared empty duplicate-bone record and clear its handle.
================
*/
void DObjShutdown(void)
{
    if (g_empty) {
        SL_RemoveRefToStringOfLen(g_empty, 17);
        g_empty = 0;
    }
}

/*
================
ConvertQuatToSkelMat

Write the rotation axes for a quaternion, accounting for its length.
A zero quaternion produces identity axes; the matrix origin is left unchanged.
================
*/
void ConvertQuatToSkelMat(const float *quat, DObjSkelMat *mat)
{
    float xx, yy, zz, len2, s;
    float xs, ys;
    float xy, xz, xw, yz, yw, zw;

    xx = quat[0] * quat[0];
    yy = quat[1] * quat[1];
    zz = quat[2] * quat[2];
    len2 = quat[3] * quat[3] + zz + yy + xx;

    if (len2 != 0.0f) {
        s = 2.0f / len2;

        xx *= s;
        yy *= s;
        zz *= s;

        xs = s * quat[0];
        xy = xs * quat[1];
        xz = xs * quat[2];
        xw = xs * quat[3];

        ys = s * quat[1];
        yz = ys * quat[2];
        yw = ys * quat[3];

        zw = quat[3] * quat[2] * s;

        mat->axis[0][0] = 1.0f - (zz + yy);
        mat->axis[0][1] = zw + xy;
        mat->axis[0][2] = xz - yw;
        mat->axis[1][0] = xy - zw;
        mat->axis[1][1] = 1.0f - (zz + xx);
        mat->axis[1][2] = yz + xw;
        mat->axis[2][0] = yw + xz;
        mat->axis[2][1] = yz - xw;
        mat->axis[2][2] = 1.0f - (yy + xx);
        mat->axis[0][3] = 0.0f;
        mat->axis[1][3] = 0.0f;
        mat->axis[2][3] = 0.0f;
    } else {
        mat->axis[0][0] = 1.0f;
        mat->axis[0][1] = 0.0f;
        mat->axis[0][2] = 0.0f;
        mat->axis[1][0] = 0.0f;
        mat->axis[1][1] = 1.0f;
        mat->axis[1][2] = 0.0f;
        mat->axis[2][0] = 0.0f;
        mat->axis[2][1] = 0.0f;
        mat->axis[2][2] = 1.0f;
        mat->axis[0][3] = 0.0f;
        mat->axis[1][3] = 0.0f;
        mat->axis[2][3] = 0.0f;
    }
}

/*
================
DObjSkelMatrixTransformVector43Equals

Transform a point in place by a bone matrix, including its translation.
================
*/
void DObjSkelMatrixTransformVector43Equals(float *v, const DObjSkelMat *matrix)
{
    /* Keep x and y until the last expression has read all input components. */
    float transformed[2];
    transformed[0] = v[0] * matrix->axis[0][0] + v[1] * matrix->axis[1][0] +
        v[2] * matrix->axis[2][0] + matrix->origin[0];
    transformed[1] = v[0] * matrix->axis[0][1] + v[1] * matrix->axis[1][1] +
        v[2] * matrix->axis[2][1] + matrix->origin[1];
    v[2] = v[0] * matrix->axis[0][2] + v[1] * matrix->axis[1][2] +
        v[2] * matrix->axis[2][2] + matrix->origin[2];
    v[0] = transformed[0];
    v[1] = transformed[1];
}

/*
================
LocalQuatMultiplyEquals

Combine two quaternion rotations and store the result in the first quaternion.
================
*/
inline void LocalQuatMultiplyEquals(float *a, const float *b)
{
    float temp[3];
    temp[0] = a[0] * b[3] + a[3] * b[0] + a[2] * b[1] - a[1] * b[2];
    temp[1] = a[1] * b[3] - a[2] * b[0] + a[3] * b[1] + a[0] * b[2];
    temp[2] = a[2] * b[3] + a[1] * b[0] - a[0] * b[1] + a[3] * b[2];
    a[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    a[0] = temp[0];
    a[1] = temp[1];
    a[2] = temp[2];
}

/*
================
LocalQuatMultiplyReverseEquals

Combine two quaternion rotations and store the result in the second quaternion.
================
*/
inline void LocalQuatMultiplyReverseEquals(const float *a, float *b)
{
    float temp[3];
    temp[0] = a[0] * b[3] + a[3] * b[0] + a[2] * b[1] - a[1] * b[2];
    temp[1] = a[1] * b[3] - a[2] * b[0] + a[3] * b[1] + a[0] * b[2];
    temp[2] = a[2] * b[3] + a[1] * b[0] - a[0] * b[1] + a[3] * b[2];
    b[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    b[0] = temp[0];
    b[1] = temp[1];
    b[2] = temp[2];
}

/*
================
DObjGetBoneIndexInternal

Find the first model containing the interned bone name and return its index
in the combined skeleton. Return -1 when no model contains the name.
================
*/
static int DObjGetBoneIndexInternal(DObj_s *obj, unsigned short name)
{
    int i, offset, index, numModels;
    XModel_s *model;
    numModels = obj->numModels;
    offset = 0;
    for (i = 0; i < numModels; i++) {
        model = obj->models[i];
        index = XModelGetBoneIndex(model, name);
        if (index >= 0)
            return index + offset;
        offset += model->data->parts->data->hierarchy->names->size;
    }
    return -1;
}

/*
================
DObjCreateDuplicateParts

Map shared bone names to their first occurrence in the combined skeleton.
The string holds a duplicate mask followed by one-based destination/source pairs.
================
*/
static void DObjCreateDuplicateParts(DObj_s *obj)
{
    char *duplicateParts;
    unsigned int len;
    int i, localBoneIndex, numBones, boneIndex, index, parentIndex;
    unsigned char rootFound;
    XModel_s *model;
    XBoneNames_s *names;

    /* Sixteen mask bytes, one-based duplicate/source pairs, then zero. */
    duplicateParts = (char *)_alloca(2 * obj->numBones + 17);
    Com_Memset(duplicateParts, 0, 16);
    boneIndex = obj->models[0]->data->parts->data->hierarchy->names->size;
    len = 16;
    for (i = 1; i < obj->numModels; ) {
        model = obj->models[i];
        if (obj->modelParents[i] == 255) {
            names = model->data->parts->data->hierarchy->names;
            numBones = names->size;
            rootFound = 0;
            for (localBoneIndex = 0; localBoneIndex < numBones; localBoneIndex++) {
                parentIndex = DObjGetBoneIndexInternal(obj, names->names[localBoneIndex]);
                index = localBoneIndex + boneIndex;
                if (parentIndex != index) {
                    if (!localBoneIndex)
                        rootFound = 1;
                    duplicateParts[len] = index + 1;
                    duplicateParts[index >> 3] |= 1 << (index & 7);
                    len++;
                    duplicateParts[len++] = parentIndex + 1;
                }
            }
            if (!rootFound) {
                Com_Printf("WARNING: Attempting to meld model, but root part '%s' of model '%s' not found in model '%s' or any of its descendants\n",
                    SL_ConvertToString(names->names[0]), model->name, obj->models[0]->name);
            }
        }
        i++;
        boneIndex += model->data->parts->data->hierarchy->names->size;
    }
    if (!g_empty) {
        int empty[5];
        Com_Memset(empty, 0, sizeof(empty));
        g_empty = SL_GetStringOfLen((const char *)empty, 0, 17, 12);
    }
    if (len > 16) {
        duplicateParts[len++] = 0;
        obj->duplicateParts = SL_GetStringOfLen(duplicateParts, 0, len, 12);
    } else {
        obj->duplicateParts = g_empty;
    }
}

/*
================
DObjGetHierarchyBits

Clear the mask and mark this bone and its ancestors, following duplicate
bones and model attachments back into the earlier models.
================
*/
void DObjGetHierarchyBits(DObj_s *obj, int boneIndex, int *partBits)
{
    int modelIndex, numModels, boneOffset, localBoneIndex, i;
    int modelOffsets[8];
    XModelPartsInfo *parts;
    const unsigned char *parents;
    const char *duplicateBits;
    const unsigned char *pos;

    for (i = 0; i < 4; i++)
        partBits[i] = 0;
    numModels = obj->numModels;
    if (!obj->duplicateParts)
        DObjCreateDuplicateParts(obj);
    duplicateBits = SL_ConvertToString(obj->duplicateParts);
    modelOffsets[0] = 0;
    modelIndex = 0;
    for (;;) {
        parts = obj->models[modelIndex]->data->parts->data;
        boneOffset = modelOffsets[modelIndex] + parts->hierarchy->names->size;
        if (boneOffset > boneIndex)
            break;
        modelIndex++;
        if (modelIndex == numModels)
            return;
        modelOffsets[modelIndex] = boneOffset;
    }
    parents = parts->hierarchy->parentList;
    /* Follow local parents one step at a time. At a model root, follow
     * attachment or duplicate links until a local parent is reached. */
    for (;;) {
        localBoneIndex = boneIndex - modelOffsets[modelIndex];
        do {
            ((unsigned char *)partBits)[boneIndex >> 3] |= 1 << (boneIndex & 7);
            if (!(duplicateBits[boneIndex >> 3] & (1 << (boneIndex & 7)))) {
                i = localBoneIndex - parts->numRootBones;
                if (i >= 0) {
                    boneIndex -= parents[i];
                    break;
                }
                boneIndex = obj->modelParents[modelIndex];
                if (boneIndex == 255)
                    return;
            } else {
                /* Duplicate-part strings contain 16 mask bytes, then bone-index pairs. */
                pos = (const unsigned char *)duplicateBits + 16;
                while (boneIndex != pos[0] - 1)
                    pos += 2;
                boneIndex = pos[1] - 1;
            }
            do {
                modelIndex--;
                localBoneIndex = boneIndex - modelOffsets[modelIndex];
            } while (localBoneIndex < 0);
            parts = obj->models[modelIndex]->data->parts->data;
            parents = parts->hierarchy->parentList;
        } while (1);
    }
}

/*
================
DObjCompleteHierarchyBits

Scan the combined bone order backward, adding parent bits to the mask.
================
*/
void DObjCompleteHierarchyBits(DObj_s *obj, int *partBits)
{
    int boneIndex, modelIndex, localIndex, newBoneIndex;
    int modelOffsets[8];
    /* A read view of the model starts while the scan fills the array. */
    const int *offsets = modelOffsets;
    XModelPartsInfo *parts;
    const unsigned char *parents;
    const char *duplicateBits;
    const unsigned char *pos;

    boneIndex = obj->numBones - 1;
    if (!obj->duplicateParts)
        DObjCreateDuplicateParts(obj);
    duplicateBits = SL_ConvertToString(obj->duplicateParts);
    modelIndex = 0;
    modelOffsets[0] = 0;
    parts = obj->models[0]->data->parts->data;
    /* Read the initial model count through its packed hierarchy field. */
    const short *const &boneCount = &parts->hierarchy->names->size;
    localIndex = *boneCount;
    while (localIndex <= boneIndex) {
        modelIndex++;
        const XModel_s *record = obj->models[modelIndex];
        modelOffsets[modelIndex] = localIndex;
        parts = record->data->parts->data;
        localIndex = offsets[modelIndex] +
            record->data->parts->data->hierarchy->names->size;
    }
    parents = parts->hierarchy->parentList;
    for (;;) {
        localIndex = boneIndex - modelOffsets[modelIndex];
        if (localIndex < 0) {
            if (--modelIndex < 0)
                return;
            parts = obj->models[modelIndex]->data->parts->data;
            parents = parts->hierarchy->parentList;
            continue;
        }
        /* Test whether the whole byte is zero, then apply the selected bit mask. */
        if ((!((char *)partBits)[boneIndex >> 3]) & (1 << (boneIndex & 7))) {
            boneIndex--;
            continue;
        }
        if (!(duplicateBits[boneIndex >> 3] & (1 << (boneIndex & 7)))) {
            localIndex -= parts->numRootBones;
            if (localIndex >= 0)
                newBoneIndex = boneIndex - parents[localIndex];
            else {
                newBoneIndex = obj->modelParents[modelIndex];
                if (newBoneIndex == 255) {
                    boneIndex--;
                    continue;
                }
            }
        } else {
            /* Duplicate-part strings contain 16 mask bytes, then bone-index pairs. */
            pos = (const unsigned char *)duplicateBits + 16;
            while (boneIndex != pos[0] - 1)
                pos += 2;
            newBoneIndex = pos[1] - 1;
        }
        ((unsigned char *)partBits)[newBoneIndex >> 3] |= 1 << (newBoneIndex & 7);
        boneIndex--;
    }
}

/*
================
DObjSkelIsBoneUpToDate

Return whether the skeleton already contains the specified bone matrix.
================
*/
qboolean DObjSkelIsBoneUpToDate(const DObj_s *obj, int boneIndex)
{
    return (qboolean)((((char *)obj->skel->skelPartBits)[boneIndex >> 3] & (1 << (boneIndex & 7))) != 0);
}

/*
================
DObjSkelAreBonesUpToDate

Return whether every bone selected by the mask has a calculated skeleton matrix.
================
*/
qboolean DObjSkelAreBonesUpToDate(const DObj_s *obj, const int *partBits)
{
    int i;
    const DSkel_s *skel;
    skel = obj->skel;
    for (i = 0; i < 4; i++) {
        if (partBits[i] & ~skel->skelPartBits[i])
            return qfalse;
    }
    return qtrue;
}

#define DOBJ_COPY_QUAT(to, from) { \
    (to)[0] = (from)[0]; (to)[1] = (from)[1]; \
    (to)[2] = (from)[2]; (to)[3] = (from)[3]; \
}
#define DOBJ_COPY_TRANS(to, from) { \
    (to)[0] = (from)[0]; (to)[1] = (from)[1]; (to)[2] = (from)[2]; \
}

/*
================
DObjCalcSkel

Evaluate requested bones that are not already present in the skeleton.
Compose rotations first, then build the matrices and translations.
================
*/
void DObjCalcSkel(const DObj_s *obj, const int *partBits)
{
    XModelPartsInfo *parts;

    DSkel_s *skel = obj->skel;
    int controlPartBits[4], ignorePartBits[4], calcPartBits[4];

    int i, j, boneIndex;
    int boneIndexHigh, boneIndexLow;
    unsigned char allSet = 1;

    for (i = 0; i < 4; i++) {
        ignorePartBits[i] = skel->skelPartBits[i] | ~partBits[i];
        if (~ignorePartBits[i])
            allSet = 0;
    }
    if (allSet)
        return;

    if (!obj->duplicateParts)
        DObjCreateDuplicateParts((DObj_s *)obj);
    const int *duplicateBits = (const int *)SL_ConvertToString(obj->duplicateParts);
    const unsigned char *duplicateParts = (const unsigned char *)(duplicateBits + 4);
    for (i = 0; i < 4; i++) {
        skel->skelPartBits[i] |= partBits[i];
        controlPartBits[i] = skel->controlPartBits[i] & ~ignorePartBits[i];
        calcPartBits[i] = duplicateBits[i] | controlPartBits[i] | ignorePartBits[i];
    }
    const unsigned char *pos = duplicateParts;
    for (i = 0; i < 4; i++)
        controlPartBits[i] |= ~calcPartBits[i];
    int numModels = obj->numModels;
    DObjSkelMat *mat = skel->mat;
    DObjAnimMat_s *sourceMat = (DObjAnimMat_s *)&mat[obj->numBones];
    DObjAnimMat_s *childMat = sourceMat;
    boneIndex = 0;
    /* Compose parent rotations and copy rotations for shared bones. */
    for (i = 0; i < numModels; i++) {
        parts = obj->models[i]->data->parts->data;
        unsigned char modelParent = obj->modelParents[i];
        if (modelParent == 255) {
            for (j = parts->numRootBones; j; j--, childMat++, boneIndex++) {
                DObjAnimMat_s &boneMat = *childMat;
                boneIndexHigh = boneIndex >> 3;
                boneIndexLow = 1 << (boneIndex & 7);
                if (!(((const char *)controlPartBits)[boneIndexHigh] & boneIndexLow) &&
                    boneIndex == pos[0] - 1) {
                    pos += 2;
                    if (!(((const char *)ignorePartBits)[boneIndexHigh] & boneIndexLow)) {
                        const DObjAnimMat_s *source = sourceMat + pos[-1] - 1;
                        DOBJ_COPY_QUAT(boneMat.quat, source->quat);
                    }
                }
            }
        } else {
            const DObjAnimMat_s *parentMat = sourceMat + modelParent;
            for (j = parts->numRootBones; j; j--, childMat++, boneIndex++) {
                DObjAnimMat_s &boneMat = *childMat;
                boneIndexHigh = boneIndex >> 3;
                boneIndexLow = 1 << (boneIndex & 7);
                if (!(((const char *)calcPartBits)[boneIndexHigh] & boneIndexLow))
                    LocalQuatMultiplyEquals(boneMat.quat, parentMat->quat);
                else if (((const char *)controlPartBits)[boneIndexHigh] & boneIndexLow)
                    LocalQuatMultiplyReverseEquals(parentMat->quat, boneMat.quat);
            }
        }
        const unsigned char *parentList = parts->hierarchy->parentList;
        for (j = parts->hierarchy->names->size - parts->numRootBones; j;
            j--, childMat++, parentList++, boneIndex++) {
            boneIndexHigh = boneIndex >> 3;
            boneIndexLow = 1 << (boneIndex & 7);
            if (!(((const char *)calcPartBits)[boneIndexHigh] & boneIndexLow))
                LocalQuatMultiplyEquals(childMat->quat, childMat[-*parentList].quat);
            else if (((const char *)controlPartBits)[boneIndexHigh] & boneIndexLow)
                LocalQuatMultiplyReverseEquals(childMat[-*parentList].quat, childMat->quat);
            else if (boneIndex == pos[0] - 1) {
                pos += 2;
                if (!(((const char *)ignorePartBits)[boneIndexHigh] & boneIndexLow)) {
                    const DObjAnimMat_s *source = sourceMat + pos[-1] - 1;
                    DOBJ_COPY_QUAT(childMat->quat, source->quat);
                }
            }
        }
    }

    /* Build object-space matrices, add bind translations and copy shared bones. */
    pos = duplicateParts;
    childMat = sourceMat;
    DObjSkelMat *childMatDest = mat;
    boneIndex = 0;
    for (i = 0; i < numModels; i++) {
        parts = obj->models[i]->data->parts->data;
        unsigned char modelParent = obj->modelParents[i];
        if (modelParent == 255) {
            for (j = parts->numRootBones; j; j--, childMat++, childMatDest++, boneIndex++) {
                DObjAnimMat_s &boneMat = *childMat;
                boneIndexHigh = boneIndex >> 3;
                boneIndexLow = 1 << (boneIndex & 7);
                if (((const char *)controlPartBits)[boneIndexHigh] & boneIndexLow) {
                    ConvertQuatToSkelMat(boneMat.quat, childMatDest);
                    DOBJ_COPY_TRANS(childMatDest->origin, boneMat.trans);
                    childMatDest->origin[3] = 0;
                } else if (boneIndex == pos[0] - 1) {
                    pos += 2;
                    if (!(((const char *)ignorePartBits)[boneIndexHigh] & boneIndexLow)) {
                        int sourceIndex = pos[-1] - 1;
                        const DObjAnimMat_s *duplicateMat = sourceMat + sourceIndex;
                        const DObjSkelMat *sourceSkel = mat + sourceIndex;
                        DOBJ_COPY_TRANS(boneMat.trans, duplicateMat->trans);
                        *childMatDest = *sourceSkel;
                    }
                }
            }
        } else {
            const DObjSkelMat *parentMat = mat + modelParent;
            for (j = parts->numRootBones; j; j--, childMat++, childMatDest++, boneIndex++) {
                boneIndexHigh = boneIndex >> 3;
                boneIndexLow = 1 << (boneIndex & 7);
                if (((const char *)controlPartBits)[boneIndexHigh] & boneIndexLow) {
                    ConvertQuatToSkelMat(childMat->quat, childMatDest);
                    DObjSkelMatrixTransformVector43Equals(childMat->trans, parentMat);
                    DOBJ_COPY_TRANS(childMatDest->origin, childMat->trans);
                    childMatDest->origin[3] = 0;
                }
            }
        }
        const float (*trans)[3] = parts->trans;
        const unsigned char *parentList = parts->hierarchy->parentList;
        for (j = parts->hierarchy->names->size - parts->numRootBones; j;
            j--, childMat++, trans++, childMatDest++, parentList++, boneIndex++) {
            boneIndexHigh = boneIndex >> 3;
            boneIndexLow = 1 << (boneIndex & 7);
            if (((const char *)controlPartBits)[boneIndexHigh] & boneIndexLow) {
                DObjAnimMat_s &boneMat = *childMat;
                ConvertQuatToSkelMat(boneMat.quat, childMatDest);
                boneMat.trans[0] += (*trans)[0];
                boneMat.trans[1] += (*trans)[1];
                boneMat.trans[2] += (*trans)[2];
                const DObjSkelMat *parentSkel = childMatDest - *parentList;
                DObjSkelMatrixTransformVector43Equals(boneMat.trans, parentSkel);
                DOBJ_COPY_TRANS(childMatDest->origin, boneMat.trans);
                childMatDest->origin[3] = 0;
            } else if (boneIndex == pos[0] - 1) {
                pos += 2;
                if (!(((const char *)ignorePartBits)[boneIndexHigh] & boneIndexLow)) {
                    int sourceIndex = pos[-1] - 1;
                    const DObjAnimMat_s *duplicateMat = sourceMat + sourceIndex;
                    const DObjSkelMat *sourceSkel = mat + sourceIndex;
                    DOBJ_COPY_TRANS(childMat->trans, duplicateMat->trans);
                    *childMatDest = *sourceSkel;
                }
            }
        }
    }
}

/*
================
DObjGetMatrixArray

Return the first skeleton matrix belonging to the selected model.
================
*/
DObjSkelMat *DObjGetMatrixArray(const DObj_s *obj, int modelIndex)
{
    return obj->skel->mat + obj->matOffset[modelIndex];
}

/*
================
DObjCreate

Attach each model to its named parent bone and assign its skeleton range.
Models with a missing attachment are left out of the object.
================
*/
void DObjCreate(const DObjModel_s *dobjModels, unsigned short numModels,
    XAnimTree_s *tree, DObj_s *obj, unsigned short entnum)
{
    int i, j, newNumModels, numBones, boneIndex, size;
    unsigned short stringValue;
    unsigned char generation;
    unsigned char *generationPtr;
    XModel_s *model;
    const char *boneName;
    const DObjModel_s *modelInfo;

    obj->tree = tree;
    obj->skel = 0;
    obj->timeStamp = 0;
    obj->entnum = entnum;
    obj->duplicateParts = 0;
    if (tree) {
        size = tree->anims->size;
        /* After infoArray, each packed buffer holds size mapping shorts,
         * one generation byte and size generation tags. */
        struct XAnimBufferLayout {
            int headerSize;
            int stride;
        };
        XAnimBufferLayout layout = {
            size * sizeof(unsigned short) + offsetof(XAnimTree_s, infoArray),
            size * (sizeof(unsigned short) + sizeof(unsigned char)) + sizeof(unsigned char)
        };
        /* Skip the tree's info array, then select one packed mapping buffer. */
        obj->animToModel = (unsigned short *)((char *)tree +
            (tree->animToModelIndex * layout.stride + layout.headerSize));
        /* The generation byte follows the mapping shorts. */
        generationPtr = (unsigned char *)&obj->animToModel[size];
        /* On generation wrap, clear stale tags before reusing generation 1. */
        generation = *generationPtr + 1;
        if (!generation) {
            generation = 1;
            Com_Memset(generationPtr + 1, 0, size);
        }
        *generationPtr = generation;
        tree->animToModelIndex = 1 - tree->animToModelIndex;
    } else {
        obj->animToModel = 0;
    }
    newNumModels = 0;
    numBones = 0;
    for (i = 0, modelInfo = dobjModels; i < numModels; i++, modelInfo++) {
        model = modelInfo->model;
        obj->models[newNumModels] = model;
        obj->modelParents[newNumModels] = 255;
        obj->boneNames[newNumModels] = modelInfo->boneNames;
        obj->matOffset[newNumModels] = numBones;
        if (i) {
            boneName = modelInfo->boneName;
            if (boneName && *boneName) {
                stringValue = SL_FindString(boneName);
                bool found = false;
                if (stringValue) {
                    for (j = 0; j < newNumModels; j++) {
                        boneIndex = XModelGetBoneIndex(obj->models[j], stringValue);
                        if (boneIndex >= 0) {
                            found = true;
                            break;
                        }
                    }
                }
                if (found) {
                    obj->modelParents[newNumModels] = obj->matOffset[j] + boneIndex;
                } else {
                    Com_Printf("WARNING: Part '%s' not found in model '%s' or any of its descendants\n",
                        boneName, obj->models[0]->name);
                    continue;
                }
            }
        }
        boneIndex = model->data->parts->data->hierarchy->names->size;
        if (numBones + boneIndex >= 128) {
            Com_Error(1, "\025dobj for xmodel '%s' has more than %d bones", obj->models[0]->name, 127);
            break;
        }
        numBones += boneIndex;
        newNumModels++;
    }
    obj->numModels = newNumModels;
    obj->numBones = numBones;
}

/*
================
DObjFree

Detach the animation tree and release any private duplicate-bone record.
The shared empty record remains available for other objects.
================
*/
void DObjFree(DObj_s *obj)
{
    XAnimTree_s *tree;
    const char *string;
    tree = obj->tree;
    if (tree) {
        tree->animToModelIndex = obj->animToModel != &tree->infoArray[tree->anims->size];
        obj->animToModel = 0;
        obj->tree = 0;
    }
    if (obj->duplicateParts) {
        if (obj->duplicateParts != g_empty) {
            string = SL_ConvertToString(obj->duplicateParts);
            SL_RemoveRefToStringOfLen(obj->duplicateParts, strlen(string + 16) + 17);
        }
        obj->duplicateParts = 0;
    }
}

/*
================
DObjGetAllocSkelSize

Return the buffer size needed for the skeleton header, bone matrices,
and local quaternion and translation records.
================
*/
int DObjGetAllocSkelSize(const DObj_s *obj)
{
    return offsetof(DSkel_s, mat) + obj->numBones * (sizeof(DObjSkelMat) + sizeof(DObjAnimMat_s));
}

/*
================
DObjSkelExists

Check for a skeleton with the requested timestamp.
A changed timestamp clears the cached skeleton pointer and flags.
================
*/
qboolean DObjSkelExists(DObj_s *obj, int timeStamp)
{
    if (obj->timeStamp == timeStamp)
        return (qboolean)(obj->skel != 0);
    obj->timeStamp = timeStamp;
    obj->skelFlags = 0;
    obj->skel = 0;
    return qfalse;
}

/*
================
DObjSkelExistsConst

Return whether a skeleton is present with the requested timestamp.
================
*/
qboolean DObjSkelExistsConst(const DObj_s *obj, int timeStamp)
{
    return (qboolean)(obj->timeStamp == timeStamp && obj->skel != 0);
}

/*
================
DObjCreateSkel

Attach the supplied skeleton buffer and clear its animation, control,
and calculated-bone masks.
================
*/
void DObjCreateSkel(DObj_s *obj, void *buf)
{
    int i;
    DSkel_s *skel;

    skel = (DSkel_s *)buf;
    obj->skel = skel;
    for (i = 0; i < 4; i++) {
        skel->animPartBits[i] = 0;
        skel->controlPartBits[i] = 0;
        skel->skelPartBits[i] = 0;
    }
}

/*
================
DObjGetNumModels

Return the number of models attached to the skeletal object.
================
*/
int DObjGetNumModels(const DObj_s *obj)
{
    return obj->numModels;
}

/*
================
DObjGetModel

Return the model at the specified attachment index.
================
*/
XModel_s *DObjGetModel(const DObj_s *obj, int modelIndex)
{
    return obj->models[modelIndex];
}

/*
================
DObjGetBounds

Copy the first model's bounds into the supplied minimum and maximum vectors.
================
*/
void DObjGetBounds(const DObj_s *obj, float *mins, float *maxs)
{
    XModelGetBounds(obj->models[0], mins, maxs);
}

/*
================
DObjGetRotTransArray

Return the local quaternion and translation records stored after
the object-space bone matrices in the skeleton buffer.
================
*/
DObjAnimMat_s *DObjGetRotTransArray(const DObj_s *obj)
{
    /* Local animation records follow the object-space skeleton matrices. */
    return (DObjAnimMat_s *)(obj->skel->mat + obj->numBones);
}

/*
================
DObjGetBoneInfo

Fill the output array with pointers to each model's bone bounds records,
in combined skeleton order.
================
*/
void DObjGetBoneInfo(const DObj_s *obj, XBoneInfo_s **boneInfo)
{
    int i, j, numBones;
    XModelPartsInfo *parts;
    for (i = 0; i < obj->numModels; i++) {
        parts = obj->models[i]->data->parts->data;
        numBones = parts->hierarchy->names->size;
        for (j = 0; j < numBones; j++, boneInfo++)
            *boneInfo = parts->boneInfo + j;
    }
}

/*
================
DObjSetRotTransIndex

Mark a requested bone's local pose as supplied, returning true when it
can be set. Bones with calculated skeleton matrices are left unchanged.
================
*/
qboolean DObjSetRotTransIndex(const DObj_s *obj, const char *partBits, int boneIndex)
{
    int index, bit;
    DSkel_s *skel;
    index = boneIndex >> 3;
    bit = 1 << (boneIndex & 7);
    if (!(partBits[index] & bit))
        return qfalse;
    skel = obj->skel;
    if (((char *)skel->skelPartBits)[index] & bit)
        return qfalse;
    ((char *)skel->animPartBits)[index] |= bit;
    return qtrue;
}

/*
================
DObjSetControlRotTransIndex

Mark a requested bone as having a supplied control pose and animation pose.
Return false if it is unrequested or its skeleton matrix is already calculated.
================
*/
qboolean DObjSetControlRotTransIndex(const DObj_s *obj, const char *partBits, int boneIndex)
{
    int index, bit;
    DSkel_s *skel;
    index = boneIndex >> 3;
    bit = 1 << (boneIndex & 7);
    if (!(partBits[index] & bit))
        return qfalse;
    skel = obj->skel;
    if (((char *)skel->skelPartBits)[index] & bit)
        return qfalse;
    ((char *)skel->controlPartBits)[index] |= bit;
    ((char *)skel->animPartBits)[index] |= bit;
    return qtrue;
}

/*
================
DObjGetNumSurfaces

Count loaded surfaces at the selected level of detail for each model.
Negative level selections and unloaded surface sets contribute nothing.
================
*/
int DObjGetNumSurfaces(const DObj_s *obj, const int *lods)
{
    int i, count;
    XModelSurfsFile_s *surfs;
    count = 0;
    for (i = obj->numModels - 1; i >= 0; i--) {
        if (lods[i] >= 0) {
            surfs = obj->models[i]->data->lodInfo[lods[i]].surfs;
            if (surfs)
                count += surfs->data->numSurfs;
        }
    }
    return count;
}

/*
================
DObjGetSurface

Return a model surface at the level of detail selected for that model.
================
*/
XSurface_s *DObjGetSurface(const DObj_s *obj, int modelIndex, int surfaceIndex, const int *lods)
{
    return obj->models[modelIndex]->data->lodInfo[lods[modelIndex]].surfs->data->surfs[surfaceIndex];
}

/*
================
DObjGetSurfaceName

Return the selected surface's material name, or DEFAULT for an unnamed material.
================
*/
const char *DObjGetSurfaceName(const DObj_s *obj, int modelIndex, int subMatIndex, const int *lods)
{
    unsigned short name;
    name = obj->models[modelIndex]->data->lodInfo[lods[modelIndex]].surfNames[subMatIndex];
    return name ? SL_ConvertToString(name) : "DEFAULT";
}

/*
================
DObjGetSurfaces

List surfaces at the selected model detail levels and clear and fill their
combined bone mask. Model-local bone bits are shifted to skeleton indices.
================
*/
void DObjGetSurfaces(const DObj_s *obj, DObjSurface_s *surfaceInfo,
    int *partBits, const int *lods)
{
    int i, j, k, wordIndex, numModels, numSurfs, lastWord, offset, shift, inverseShift;
    XModelInfo *model;
    XModelSurfsFile_s *surfsFile;
    XSurface_s **surfs;
    XModelSurfs_s *modelSurfs;

    Com_Memset(partBits, 0, 16);
    numModels = obj->numModels;
    for (i = 0; i < numModels; i++) {
        if (lods[i] < 0)
            continue;
        model = obj->models[i]->data;
        surfsFile = model->lodInfo[lods[i]].surfs;
        if (!surfsFile)
            continue;
        modelSurfs = surfsFile->data;
        lastWord = (model->parts->data->hierarchy->names->size - 1) >> 5;
        numSurfs = modelSurfs->numSurfs;
        shift = obj->matOffset[i];
        surfs = modelSurfs->surfs;
        offset = shift >> 5;
        shift &= 31;
        /* Move each model-local bone mask into the combined skeleton. */
        if (shift) {
            inverseShift = 32 - shift;
            for (j = 0; j < numSurfs; j++, surfaceInfo++, surfs++) {
                surfaceInfo->surfaceIndex = j;
                surfaceInfo->modelIndex = i;
                wordIndex = offset;
                partBits[wordIndex++] |= (*surfs)->partBits[0] << shift;
                for (k = 0; k < lastWord; wordIndex++, k++)
                    partBits[wordIndex] |= ((unsigned int)(*surfs)->partBits[k] >> inverseShift) |
                        ((*surfs)->partBits[k + 1] << shift);
                partBits[wordIndex] |= (unsigned int)(*surfs)->partBits[k] >> inverseShift;
            }
        } else {
            for (j = 0; j < numSurfs; j++, surfaceInfo++, surfs++) {
                surfaceInfo->surfaceIndex = j;
                surfaceInfo->modelIndex = i;
                wordIndex = offset;
                for (k = 0; k <= lastWord; wordIndex++, k++)
                    partBits[wordIndex] |= (*surfs)->partBits[k];
            }
        }
    }
}

/*
================
DObjGetBoneIndex

Find a bone by its lowercase interned name and return its combined index.
Return -1 if the name is unknown or absent from the attached models.
================
*/
int DObjGetBoneIndex(DObj_s *obj, const char *name)
{
    unsigned short stringValue;
    stringValue = SL_FindLowercaseString(name);
    if (stringValue)
        return DObjGetBoneIndexInternal(obj, stringValue);
    return -1;
}

/*
================
DObjGetBoneName

Return the name of a bone in combined skeleton order, or null if
the index lies beyond the attached models.
================
*/
const char *DObjGetBoneName(const DObj_s *obj, int index)
{
    int i, offset, numModels, localIndex;
    XBoneNames_s *names;
    XModel_s *model;
    numModels = obj->numModels;
    offset = 0;
    for (i = 0; i < numModels; i++) {
        model = obj->models[i];
        names = model->data->parts->data->hierarchy->names;
        localIndex = index - offset;
        if (localIndex < model->data->parts->data->hierarchy->names->size)
            return SL_ConvertToString(names->names[localIndex]);
        offset += names->size;
    }
    return 0;
}

/*
================
DObjGetTree

Return the animation tree attached to the skeletal object.
================
*/
XAnimTree_s *DObjGetTree(const DObj_s *obj)
{
    return obj->tree;
}

/*
================
DObjBad

Return whether any attached model uses the fallback model data.
================
*/
qboolean DObjBad(const DObj_s *obj)
{
    int i;
    for (i = obj->numModels - 1; i >= 0; i--)
        if (XModelBad(obj->models[i]))
            return qtrue;
    return qfalse;
}

/*
================
DObjNumBones

Return the total number of bones in the combined skeleton.
================
*/
int DObjNumBones(const DObj_s *obj)
{
    return obj->numBones;
}

/* Engine trace result used by the model geometry query. */
typedef struct trace_s {
    float fraction;
    float endpos[3];
    float normal[3];
    int surfaceFlags;
    int contents;
    void *material;
    unsigned short entityNum;
    unsigned short partGroup;
    unsigned short partName;
    unsigned char allsolid;
    unsigned char startsolid;
} trace_t;

/*
================
LocalMatrixTransformVector43

Transform a bone-local point by the rotation and translation of its matrix.
================
*/
static void LocalMatrixTransformVector43(const float *in, const DObjSkelMat *matrix, float *out)
{
    out[0] = in[0] * matrix->axis[0][0] + in[1] * matrix->axis[1][0] +
        in[2] * matrix->axis[2][0] + matrix->origin[0];
    out[1] = in[0] * matrix->axis[0][1] + in[1] * matrix->axis[1][1] +
        in[2] * matrix->axis[2][1] + matrix->origin[1];
    out[2] = in[0] * matrix->axis[0][2] + in[1] * matrix->axis[1][2] +
        in[2] * matrix->axis[2][2] + matrix->origin[2];
}

/*
================
LocalInvMatrixTransformVector43

Convert a point to bone-local coordinates by removing the matrix translation
and applying the transposed rotation axes.
================
*/
static void LocalInvMatrixTransformVector43(const float *in, const DObjSkelMat *matrix, float *out)
{
    float local[3];
    local[0] = in[0] - matrix->origin[0];
    local[1] = in[1] - matrix->origin[1];
    local[2] = in[2] - matrix->origin[2];
    out[0] = local[0] * matrix->axis[0][0] + local[1] * matrix->axis[0][1] + local[2] * matrix->axis[0][2];
    out[1] = local[0] * matrix->axis[1][0] + local[1] * matrix->axis[1][1] + local[2] * matrix->axis[1][2];
    out[2] = local[0] * matrix->axis[2][0] + local[1] * matrix->axis[2][1] + local[2] * matrix->axis[2][2];
}

/*
================
DObjTraceline

Trace against bone bounds, preferring the hit with the highest part priority.
Sphere rejection precedes the six-plane clip in each bone's local space.
================
*/
void DObjTraceline(DObj_s *obj, const float *start, const float *end,
    const unsigned char *priorityMap, DObjTrace_s *results)
{
    float delta[3];
    VectorSubtract(end, start, delta);
    float inverseLengthSquared = 1.0f / DotProduct(delta, delta);
    DObjSkelMat *boneMatrix = DObjGetMatrixArray(obj, 0);
    unsigned int bestPriority = 2;

    results->surfaceFlags = 0;
    results->startsolid = 0;
    results->allsolid = 0;
    results->partName = 0;
    results->partGroup = 0;
    VectorClear(results->normal);

    const unsigned char *duplicate = (const unsigned char *)SL_ConvertToString(obj->duplicateParts) + 16;
    unsigned short partGroups[128];
    int globalBoneIndex = 0;
    int hitT = -1;
    float hitSign = 0;
    for (int modelNum = 0; modelNum < obj->numModels; modelNum++) {
        const XModelPartsInfo *parts = obj->models[modelNum]->data->parts->data;
        const XBoneNames_s *boneNames = parts->hierarchy->names;
        int numBones = boneNames->size;
        for (int boneNum = 0; boneNum < numBones; boneNum++, boneMatrix++, globalBoneIndex++) {
            /* Priority 1 inherits the parent or shared bone's classification. */
            unsigned short partGroup = parts->partClassification[boneNum];
            unsigned int priority = priorityMap[partGroup];
            if (globalBoneIndex == duplicate[0] - 1) {
                duplicate += 2;
                if (priority == 1) {
                    partGroup = partGroups[duplicate[-1] - 1];
                    priority = priorityMap[partGroup];
                }
            } else if (priority == 1) {
                if (boneNum < parts->numRootBones) {
                    if (obj->modelParents[modelNum] != 255)
                        partGroup = partGroups[obj->modelParents[modelNum]];
                    else
                        partGroup = 0;
                } else {
                    unsigned char parent = parts->hierarchy->parentList[boneNum - parts->numRootBones];
                    partGroup = partGroups[globalBoneIndex - parent];
                }
                priority = priorityMap[partGroup];
            }
            partGroups[globalBoneIndex] = partGroup;

            const XBoneInfo_s *boneInfo = &parts->boneInfo[boneNum];
            if (boneInfo->radiusSquared == 0)
                continue;
            if (bestPriority > priority)
                continue;

            /* Reject bones whose bounding sphere cannot improve the trace. */
            float center[3], fromCenter[3];
            LocalMatrixTransformVector43(boneInfo->offset, boneMatrix, center);
            VectorSubtract(start, center, fromCenter);
            float closestFraction = -DotProduct(fromCenter, delta) * inverseLengthSquared;
            float distanceSquared;
            if (closestFraction < 1) {
                if (closestFraction > 0) {
                    float closest[3];
                    VectorMA(fromCenter, closestFraction, delta, closest);
                    distanceSquared = DotProduct(closest, closest);
                } else {
                    distanceSquared = DotProduct(fromCenter, fromCenter);
                }
            } else {
                float closest[3];
                VectorSubtract(end, center, closest);
                distanceSquared = DotProduct(closest, closest);
            }
            float radiusSquared = boneInfo->radiusSquared - distanceSquared;
            if (radiusSquared <= 0)
                continue;
            if (bestPriority == priority) {
                if (closestFraction - (float)sqrt(radiusSquared * inverseLengthSquared) >= results->fraction)
                    continue;
            }

            /* Clip the segment against the six planes of the bone bounds. */
            float localStart[3], localEnd[3];
            LocalInvMatrixTransformVector43(start, boneMatrix, localStart);
            LocalInvMatrixTransformVector43(end, boneMatrix, localEnd);
            float enter = 0;
            float leave = results->fraction;
            unsigned char startSolid = 1;
            unsigned char endSolid = 1;
            const float *bounds = boneInfo->bounds[0];
            for (float sign = -1; ; sign = 1, bounds += 3) {
                for (int axis = 0; axis < 3; axis++) {
                    float startDist = (localStart[axis] - bounds[axis]) * sign;
                    float endDist = (localEnd[axis] - bounds[axis]) * sign;
                    if (startDist > 0) {
                        if (endDist > 0)
                            goto nextBone;
                        startSolid = 0;
                        float dist = startDist - endDist;
                        if (startDist > enter * dist) {
                            enter = startDist / dist;
                            if (enter >= leave)
                                goto nextBone;
                            hitT = axis;
                            hitSign = sign;
                        }
                    } else if (endDist > 0) {
                        endSolid = 0;
                        float dist = startDist - endDist;
                        if (startDist > leave * dist) {
                            leave = startDist / dist;
                            if (enter >= leave)
                                goto nextBone;
                        }
                    }
                }
                if (sign == 1)
                    break;
            }

            if (startSolid) {
                results->startsolid = 1;
                if (endSolid) {
                    results->allsolid = 1;
                    results->fraction = 0;
                    results->partName = boneNames->names[boneNum];
                    results->partGroup = partGroup;
                    VectorClear(results->normal);
                    return;
                }
                continue;
            }
            if (bestPriority == priority) {
                if (enter >= results->fraction)
                    continue;
            } else {
                bestPriority = priority;
            }
            results->fraction = enter;
            results->partName = boneNames->names[boneNum];
            results->partGroup = partGroup;
            VectorScale(boneMatrix->axis[hitT], hitSign, results->normal);

nextBone:
            continue;
        }
    }
}


/*
================
DObjGeomTraceline

Trace the segment against each attached model using its skeleton matrices.
Copy the resulting fraction, surface flags, normal and hit bone name to the result.
================
*/
void DObjGeomTraceline(DObj_s *obj, float *localStart, float *localEnd,
    int contentmask, DObjTrace_s *results)
{
    int i, partIndex;
    XModel_s *model;
    XBoneNames_s *names;
    DObjSkelMat *mat;
    trace_t trace;

    mat = obj->skel->mat + obj->matOffset[0];
    trace.fraction = results->fraction;
    results->partName = 0;
    results->partGroup = 0;
    results->startsolid = 0;
    results->allsolid = 0;
    trace.normal[0] = trace.normal[1] = trace.normal[2] = trace.surfaceFlags = 0;
    for (i = 0; i < obj->numModels; i++) {
        model = obj->models[i];
        names = model->data->parts->data->hierarchy->names;
        partIndex = XModelTraceLine(model, &trace, mat, localStart, localEnd, contentmask);
        if (partIndex >= 0)
            results->partName = names->names[partIndex];
        mat += names->size;
    }
    results->fraction = trace.fraction;
    results->surfaceFlags = trace.surfaceFlags;
    results->normal[0] = trace.normal[0];
    results->normal[1] = trace.normal[1];
    results->normal[2] = trace.normal[2];
}

/*
================
DObjGetLodForDist

Return the selected model's level of detail for the supplied distance.
================
*/
int DObjGetLodForDist(const DObj_s *obj, int modelIndex, float dist)
{
    return XModelGetLodForDist(obj->models[modelIndex], dist);
}

/*
================
DObjCalcAnim

Initialize requested local pose records from the bind rotations.
Translation deltas start at zero; the skeleton pass adds bind translations.
================
*/
void DObjCalcAnim(const DObj_s *obj, const int *partBits)
{
    int i, j, boneIndex;
    int ignorePartBits[4];
    unsigned char allSet;
    DSkel_s *skel;
    DObjAnimMat_s *mat;
    XModelPartsInfo *parts;
    const short (*quats)[4];

    skel = obj->skel;
    allSet = 1;
    for (i = 0; i < 4; i++) {
        ignorePartBits[i] = skel->animPartBits[i] | ~partBits[i];
        if (~ignorePartBits[i])
            allSet = 0;
    }
    if (allSet)
        return;
    for (i = 0; i < 4; i++)
        skel->animPartBits[i] |= partBits[i];
    mat = (DObjAnimMat_s *)(skel->mat + obj->numBones);
    boneIndex = 0;
    for (i = 0; i < obj->numModels; i++) {
        parts = obj->models[i]->data->parts->data;
        for (j = parts->numRootBones; j; j--, mat++, boneIndex++) {
            if (!(((char *)ignorePartBits)[boneIndex >> 3] & (1 << (boneIndex & 7)))) {
                mat->quat[0] = 0;
                mat->quat[1] = 0;
                mat->quat[2] = 0;
                mat->quat[3] = 1;
                mat->trans[0] = mat->trans[1] = mat->trans[2] = 0;
            }
        }
        quats = parts->quats;
        for (j = parts->hierarchy->names->size - parts->numRootBones;
             j; j--, mat++, boneIndex++, quats++) {
            if (!(((char *)ignorePartBits)[boneIndex >> 3] & (1 << (boneIndex & 7)))) {
                mat->quat[0] = (*quats)[0] * (1.0f / 32767.0f);
                mat->quat[1] = (*quats)[1] * (1.0f / 32767.0f);
                mat->quat[2] = (*quats)[2] * (1.0f / 32767.0f);
                mat->quat[3] = (*quats)[3] * (1.0f / 32767.0f);
                mat->trans[0] = mat->trans[1] = mat->trans[2] = 0;
            }
        }
    }
}

/*
================
DObjHasContents

Return whether any attached model has a collision content bit in the supplied mask.
================
*/
qboolean DObjHasContents(const DObj_s *obj, int contents)
{
    int i;
    for (i = 0; i < obj->numModels; i++)
        if (XModelGetContents(obj->models[i]) & contents)
            return qtrue;
    return qfalse;
}

/*
================
DObjDumpInfo

Print attached models, combined bone names and duplicate-bone mappings.
A null object produces a short diagnostic instead.
================
*/
void DObjDumpInfo(const DObj_s *obj)
{
    int i, offset, numModels, numBones;
    XModel_s *model;
    const unsigned char *duplicates;
    if (!obj) {
        Com_Printf("No Dobj\n");
        return;
    }
    Com_Printf("\nModels:\n");
    numModels = obj->numModels;
    offset = 0;
    for (i = 0; i < numModels; i++) {
        model = obj->models[i];
        Com_Printf("%d: '%s'\n", offset, model->name);
        offset += model->data->parts->data->hierarchy->names->size;
    }
    Com_Printf("\nBones:\n");
    numBones = obj->numBones;
    for (i = 0; i < numBones; i++)
        Com_Printf("Bone %d: '%s'\n", i, DObjGetBoneName(obj, i));

    if (obj->duplicateParts) {
        Com_Printf("\nPart duplicates:\n");
        duplicates = (const unsigned char *)SL_ConvertToString(obj->duplicateParts) + 16;
        while (*duplicates) {
            Com_Printf("%d ('%s') -> %d ('%s')\n", duplicates[0] - 1,
                DObjGetBoneName(obj, duplicates[0] - 1), duplicates[1] - 1,
                DObjGetBoneName(obj, duplicates[1] - 1));
            duplicates += 2;
        }
    } else {
        Com_Printf("\nNo part duplicates.\n");
    }
    Com_Printf("\n");
}
