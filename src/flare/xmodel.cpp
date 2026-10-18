/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Model loading, surface access and skeletal vertex transforms.
===========================================================================
*/
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include "xmodel.h"
#include "xmodel_records.h"
#include "code/game/surfaceflags.h"
#include "flare/scr_stringlist.h"
#include "code/qcommon/qcommon.h"

/* Backing storage for the fallback model, its parts and one triangular surface. */
struct XModelDefault {
    XBoneNames_s boneNames;
    XBoneHierarchy_s hierarchy;
    XSurface_s surface;
    XSurface_s *surfaceList[1];
    XModelInfo modelInfo;
    XModelPartsInfo parts;
    XModelSurfs_s surfs;
    XBoneInfo_s boneInfo;
    XModel_s model;
    XModelPartsFile_s partsFile;
    XModelSurfsFile_s surfsFile;
    XRigidVertex_s verts[3];
    float texCoords[3][2];
    unsigned short reserved; /* Reserved word before the triangle indices. */
    unsigned short tris[3];
    unsigned char partClassification;
    unsigned short surfName;
};

static XModelDefault g_default;
static float xmodelTestLodDist = 0;
static unsigned char xmodelTestLods = 0;
static XModelLodInfo_s xmodelTestLodInfo[3] = {0};
static XModelSurfs_s *xmodelSurfs = 0;
static int xmodelEnforceExist = 0;
static int xmodelOptimize = 1;

/*
================
XModelSetOptimize

Set the model optimization option used when reporting missing skeleton parts.
================
*/
void XModelSetOptimize(int optimize)
{
    xmodelOptimize = optimize;
}

/*
================
XModelBad

Return whether the model uses the shared fallback model data.
================
*/
qboolean XModelBad(const XModel_s *model)
{
    return (qboolean)(model->data == &g_default.modelInfo);
}

/*
================
XModelEnforceExist

Store the option controlling whether model existence is required.
================
*/
void XModelEnforceExist(int enforce)
{
    xmodelEnforceExist = enforce;
}

/*
================
ReadQuat

Read three little-endian quantized quaternion components and recover
the nonnegative fourth component, clamping it to zero if necessary.
================
*/
void ReadQuat(const short *in, short *out)
{
    int remaining;
    int x, y, z;
    out[0] = LittleShort(in[0]);
    out[1] = LittleShort(in[1]);
    out[2] = LittleShort(in[2]);
    x = out[0];
    y = out[1];
    z = out[2];
    remaining = 32767 * 32767 - (x*x + y*y + z*z);
    /* Recover the nonnegative fourth component of the quantized quaternion. */
    out[3] = remaining > 0 ? (int)floor(sqrt((double)remaining) + 0.5) : 0;
}

/*
================
ReadQuat2

Read one little-endian quantized rotation component and recover
its nonnegative companion on the unit circle.
================
*/
void ReadQuat2(const short *in, short *out)
{
    int remaining;
    out[0] = LittleShort(in[0]);
    remaining = 32767 * 32767 - out[0] * out[0];
    out[1] = remaining > 0 ? (int)floor(sqrt((double)remaining) + 0.5) : 0;
}

/*
================
ConvertQuatToMat43

Replace the quaternion at the start of the buffer with three rotation axes.
A zero quaternion produces identity axes; the fourth entry of each row is preserved.
================
*/
void ConvertQuatToMat43(float quatAndMatrix[12])
{
    float xx, yy, zz, len2, s;
    float xs, ys;
    float xy, xz, xw, yz, yw, zw;

    xx = quatAndMatrix[0] * quatAndMatrix[0];
    yy = quatAndMatrix[1] * quatAndMatrix[1];
    zz = quatAndMatrix[2] * quatAndMatrix[2];
    len2 = quatAndMatrix[3] * quatAndMatrix[3] + zz + yy + xx;

    if (len2 != 0.0f) {
        s = 2.0f / len2;

        xx *= s;
        yy *= s;
        zz *= s;

        xs = s * quatAndMatrix[0];
        xy = xs * quatAndMatrix[1];
        xz = xs * quatAndMatrix[2];
        xw = xs * quatAndMatrix[3];

        ys = s * quatAndMatrix[1];
        yz = ys * quatAndMatrix[2];
        yw = ys * quatAndMatrix[3];

        zw = quatAndMatrix[3] * quatAndMatrix[2] * s;

        quatAndMatrix[0] = 1.0f - (zz + yy);
        quatAndMatrix[1] = zw + xy;
        quatAndMatrix[2] = xz - yw;
        quatAndMatrix[4] = xy - zw;
        quatAndMatrix[5] = 1.0f - (zz + xx);
        quatAndMatrix[6] = yz + xw;
        quatAndMatrix[8] = yw + xz;
        quatAndMatrix[9] = yz - xw;
        quatAndMatrix[10] = 1.0f - (yy + xx);
    } else {
        quatAndMatrix[0] = 1.0f;
        quatAndMatrix[1] = 0.0f;
        quatAndMatrix[2] = 0.0f;
        quatAndMatrix[4] = 0.0f;
        quatAndMatrix[5] = 1.0f;
        quatAndMatrix[6] = 0.0f;
        quatAndMatrix[8] = 0.0f;
        quatAndMatrix[9] = 0.0f;
        quatAndMatrix[10] = 1.0f;
    }
}

/* Surface storage belongs to the caller-supplied arena allocator. */
/*
================
XModelSurfsFree

Leave surface storage intact because its lifetime belongs to the supplied arena allocator.
================
*/
void XModelSurfsFree(XModelSurfsFile_s *modelSurfs)
{
}

/*
================
XModelPartsFree

Release the interned bone names belonging to a model parts file.
================
*/
void XModelPartsFree(XModelPartsFile_s *modelParts)
{
    int i, numBones;
    XBoneNames_s *boneNames;
    unsigned short *names;
    boneNames = modelParts->data->hierarchy->names;
    numBones = boneNames->size;
    names = boneNames->names;
    for (i = 0; i < numBones; i++)
        SL_RemoveRefToString(names[i]);
}

/*
================
XModelFree

Release interned surface material names for every detail level
and clear the material-name array pointers.
================
*/
void XModelFree(XModel_s *model)
{
    int i, j;
    XModelInfo *info;
    info = model->data;
    for (i = 0; i < 3; i++) {
        if (info->lodInfo[i].surfNames) {
            for (j = 0; j < info->lodInfo[i].numsurfs; j++)
                SL_RemoveRefToString(info->lodInfo[i].surfNames[j]);
            info->lodInfo[i].surfNames = 0;
        }
    }
}


#define XMODEL_READ_SHORT(value, cursor) { \
    (value) = LittleShort(*(short *)*(cursor)); *(cursor) += 2; \
}
#define XMODEL_READ_FLOAT(value, cursor) { \
    (value) = LittleFloat(*(float *)*(cursor)); *(cursor) += 4; \
}
#define XMODEL_READ_VEC3(value, cursor) { \
    XMODEL_READ_FLOAT((value)[0], cursor); \
    XMODEL_READ_FLOAT((value)[1], cursor); \
    XMODEL_READ_FLOAT((value)[2], cursor); \
}
#define XMODEL_READ_VEC2(value, cursor) { \
    XMODEL_READ_FLOAT((value)[0], cursor); \
    XMODEL_READ_FLOAT((value)[1], cursor); \
}

/*
================
ReadBlend

Read a bone influence and its local position, advancing the file cursor.
Mark the bone in the surface mask and store its matrix byte offset.
================
*/
static void ReadBlend(XSurface_s *surface, XSimpleBlendInfo_s *blend, char **pos)
{
    int boneIndex;
    boneIndex = LittleShort(*(short *)*pos);
    /* partBits is a 128-bone mask, addressed one byte at a time. */
    ((unsigned char *)surface->partBits)[boneIndex >> 3] |= 1 << (boneIndex & 7);
    blend->boneOffset = boneIndex * sizeof(DObjSkelMat);
    *pos += 2;
    XMODEL_READ_VEC3(blend->xyz, pos);
}

#define XSTRIP_EMIT_TRI(tri, out) { \
    if ((tri)[0] != (tri)[1] && (tri)[0] != (tri)[2] && (tri)[1] != (tri)[2]) { \
        (out)[0] = (tri)[0]; (out)[1] = (tri)[1]; (out)[2] = (tri)[2]; (out) += 3; \
    } \
}

/*
================
XSurfaceUnstrip

Expand triangle strips into triangle indices, preserving their winding
and omitting triangles with repeated vertices.
================
*/
static void XSurfaceUnstrip(XStripInfo_s *stripInfo, unsigned short *out)
{
    int i, j, numVerts;
    unsigned short tri[3];
    unsigned short *verts;

    /* Alternate the replaced corner to preserve triangle-strip winding. */
    verts = stripInfo->stripIndexes;
    for (i = 0; i < stripInfo->numStrips; i++) {
        numVerts = stripInfo->numVerts[i];
        tri[0] = verts[0];
        tri[1] = verts[1];
        tri[2] = verts[2];
        verts += 3;
        XSTRIP_EMIT_TRI(tri, out);
        for (j = 3; j < numVerts;) {
            tri[0] = tri[2];
            tri[2] = *verts++;
            XSTRIP_EMIT_TRI(tri, out);
            if (++j >= numVerts)
                break;
            tri[1] = tri[2];
            tri[2] = *verts++;
            XSTRIP_EMIT_TRI(tri, out);
            j++;
        }
    }
}

/*
================
XModelReadSurface

Read rigid or blended vertex records and expand the triangle strips.
Additional blends are stored separately from each vertex's primary influence.
================
*/
static void XModelReadSurface(XSurface_s *surface, char **pos, void *(*alloc)(unsigned int))
{
    int boneIndex, numBlends, numSingleVerts, i, vertIndex, neededIndexes, stripSize;
    XStripInfo_s strips;
    unsigned short *indexes;
    float *vertex;
    float (*texCoords)[2];
    XSimpleBlendInfo_s *blend;
    XBlendVertex_s *blendVertex;

    surface->tileMode = **pos;
    (*pos)++;
    XMODEL_READ_SHORT(surface->numVerts, pos);
    XMODEL_READ_SHORT(surface->numTris, pos);
    XMODEL_READ_SHORT(strips.numStrips, pos);
    XMODEL_READ_SHORT(boneIndex, pos);
    Com_Memset(surface->partBits, 0, 16);
    if (boneIndex == -1) {
        XMODEL_READ_SHORT(numBlends, pos);
        XMODEL_READ_SHORT(numSingleVerts, pos);
        surface->boneIndex = -1;
        surface->blendInfo = alloc(surface->numVerts * sizeof(XWeightedVertex_s) - numSingleVerts * sizeof(float));
        surface->vertexInfo = alloc(numBlends * sizeof(XSimpleBlendInfo_s));
        surface->texCoords = (float (*)[2])alloc(surface->numVerts * sizeof(*surface->texCoords));
    } else {
        surface->boneIndex = boneIndex * sizeof(DObjSkelMat);
        /* Rigid surfaces use the same byte-addressed bone mask. */
        ((unsigned char *)surface->partBits)[boneIndex >> 3] |= 1 << (boneIndex & 7);
        numBlends = 0;
        surface->blendInfo = alloc(surface->numVerts * sizeof(XRigidVertex_s));
        surface->vertexInfo = 0;
        surface->texCoords = (float (*)[2])alloc(surface->numVerts * sizeof(*surface->texCoords));
    }
    neededIndexes = 0;
    stripSize = 0;
    for (i = strips.numStrips; i > 0; i--) {
        unsigned char numVerts = (unsigned char)(*pos)[stripSize];
        neededIndexes += numVerts;
        stripSize += numVerts * 2 + 1;
    }
    strips.numVerts = (unsigned char *)_alloca(strips.numStrips);
    strips.stripIndexes = (unsigned short *)_alloca(neededIndexes * sizeof(unsigned short));
    indexes = strips.stripIndexes;
    for (i = 0; i < strips.numStrips; i++) {
        unsigned char numVerts;
        numVerts = **pos;
        strips.numVerts[i] = numVerts;
        (*pos)++;
        memcpy(indexes, *pos, numVerts * sizeof(unsigned short));
        *pos += numVerts * sizeof(unsigned short);
        indexes += numVerts;
    }
    vertex = (float *)surface->blendInfo;
    texCoords = surface->texCoords;
    for (vertIndex = 0; vertIndex < surface->numVerts; vertIndex++) {
        XMODEL_READ_VEC3(vertex, pos);
        XMODEL_READ_VEC2(*texCoords, pos);
        if (boneIndex == -1) {
            blendVertex = (XBlendVertex_s *)vertex;
            XMODEL_READ_SHORT(blendVertex->numBlends, pos);
            /* The first blend stores its position and bone offset in the
             * vertex record; only multi-blend vertices carry a weight. */
            ReadBlend(surface, (XSimpleBlendInfo_s *)blendVertex->xyz, pos);
            if (blendVertex->numBlends) {
                XMODEL_READ_FLOAT(((XWeightedVertex_s *)vertex)->weight, pos);
                vertex += sizeof(XWeightedVertex_s) / sizeof(float);
            } else {
                vertex += sizeof(XBlendVertex_s) / sizeof(float);
            }
            texCoords++;
        } else {
            XMODEL_READ_VEC3(vertex + 3, pos);
            vertex += sizeof(XRigidVertex_s) / sizeof(float);
            texCoords++;
        }
    }
    blend = (XSimpleBlendInfo_s *)surface->vertexInfo;
    for (; numBlends > 0; numBlends--, blend++) {
        ReadBlend(surface, blend, pos);
        XMODEL_READ_FLOAT(blend->weight, pos);
    }
    /* Pad an odd triangle count with a degenerate triangle for paired copies. */
    if (surface->numTris & 1) {
        surface->numTris++;
        surface->triIndexes = (unsigned short *)alloc(surface->numTris * 3 * sizeof(unsigned short));
        XSurfaceUnstrip(&strips, surface->triIndexes);
        surface->triIndexes[surface->numTris * 3 - 3] = surface->triIndexes[surface->numTris * 3 - 4];
        surface->triIndexes[surface->numTris * 3 - 2] = surface->triIndexes[surface->numTris * 3 - 4];
        surface->triIndexes[surface->numTris * 3 - 1] = surface->triIndexes[surface->numTris * 3 - 4];
    } else {
        surface->triIndexes = (unsigned short *)alloc(surface->numTris * 3 * sizeof(unsigned short));
        XSurfaceUnstrip(&strips, surface->triIndexes);
    }
}

/*
================
XModelCreateDefaultSurface

Initialize the shared fallback surface as one triangle with three
zero positions and texture coordinates, bound to the first bone.
================
*/
static void XModelCreateDefaultSurface(XSurface_s *surface)
{
    int i;
    XRigidVertex_s *verts;
    float (*texCoords)[2];
    surface->numTris = 1;
    surface->numVerts = 3;
    Com_Memset(surface->partBits, 0, sizeof(surface->partBits));
    surface->blendInfo = g_default.verts;
    surface->texCoords = g_default.texCoords;
    surface->boneIndex = 0;
    surface->vertexInfo = 0;
    surface->triIndexes = g_default.tris;
    g_default.tris[0] = 0;
    g_default.tris[1] = 1;
    g_default.tris[2] = 2;
    verts = (XRigidVertex_s *)surface->blendInfo;
    texCoords = surface->texCoords;
    for (i = 0; i < surface->numVerts; verts++, texCoords++, i++) {
        verts->normal[0] = 1;
        verts->normal[1] = 0;
        verts->normal[2] = 0;
        (*texCoords)[0] = 0;
        (*texCoords)[1] = 0;
        verts->xyz[0] = 0;
        verts->xyz[1] = 0;
        verts->xyz[2] = 0;
    }
}

/*
================
XModelCreateDefaultParts

Initialize and return the shared fallback skeleton with one root bone
and bounds extending sixteen units in each direction.
================
*/
static XModelPartsFile_s *XModelCreateDefaultParts(void)
{
    g_default.boneNames.size = 1;
    g_default.parts.quats = 0;
    g_default.parts.trans = 0;
    g_default.partClassification = 0;
    g_default.boneNames.names[0] = 0;
    g_default.hierarchy.names = &g_default.boneNames;
    g_default.parts.hierarchy = &g_default.hierarchy;
    g_default.parts.numRootBones = 1;
    g_default.parts.partClassification = &g_default.partClassification;
    g_default.boneInfo.bounds[0][0] = -16;
    g_default.boneInfo.bounds[0][1] = -16;
    g_default.boneInfo.bounds[0][2] = -16;
    g_default.boneInfo.bounds[1][0] = 16;
    g_default.boneInfo.bounds[1][1] = 16;
    g_default.boneInfo.bounds[1][2] = 16;
    g_default.parts.boneInfo = &g_default.boneInfo;
    g_default.partsFile.data = &g_default.parts;
    g_default.partsFile.free = 0;
    g_default.partsFile.name = "DEFAULT";
    return &g_default.partsFile;
}

/*
================
XModelCreateDefaultSurfs

Initialize and return the shared surface file containing the fallback triangle.
================
*/
static XModelSurfsFile_s *XModelCreateDefaultSurfs(void)
{
    g_default.surfs.numSurfs = 1;
    g_default.surfaceList[0] = &g_default.surface;
    g_default.surfs.surfs = g_default.surfaceList;
    XModelCreateDefaultSurface(&g_default.surface);
    g_default.surfsFile.data = &g_default.surfs;
    g_default.surfsFile.free = 0;
    g_default.surfsFile.name = "DEFAULT";
    return &g_default.surfsFile;
}

/*
================
XModelCreateDefault

Point a model at the shared fallback skeleton and triangle surface.
Configure one detail level and no material-name references.
================
*/
static void XModelCreateDefault(XModel_s *model)
{
    int i;
    g_default.modelInfo.parts = XModelCreateDefaultParts();
    for (i = 0; i < 3; i++) {
        g_default.modelInfo.lodInfo[i].surfs = 0;
        g_default.modelInfo.lodInfo[i].filename = "";
        g_default.modelInfo.lodInfo[i].dist = 0;
        g_default.modelInfo.lodInfo[i].numsurfs = 1;
        g_default.modelInfo.lodInfo[i].surfNames = &g_default.surfName;
        memset(g_default.modelInfo.lodInfo[i].surfNames, 0, sizeof(*g_default.modelInfo.lodInfo[i].surfNames));
    }
    g_default.modelInfo.lodInfo[0].surfs = XModelCreateDefaultSurfs();
    model->free = 0;
    model->data = &g_default.modelInfo;
    g_default.modelInfo.collLod = 0;
    g_default.modelInfo.numLods = 1;
}

/*
================
XModelSurfsCloneSurfs

Allocate a surface-file wrapper and a copy of its surface pointer array.
The surface records remain shared, and the new set joins the tracked surface list.
================
*/
static XModelSurfsFile_s *XModelSurfsCloneSurfs(XModelSurfsFile_s *source, void *(*alloc)(unsigned int))
{
    XModelSurfs_s *surfs;
    XModelSurfsFile_s *file;
    surfs = (XModelSurfs_s *)alloc(sizeof(*surfs));
    surfs->numSurfs = source->data->numSurfs;
    surfs->surfs = (XSurface_s **)alloc(surfs->numSurfs * sizeof(*surfs->surfs));
    Com_Memcpy(surfs->surfs, source->data->surfs, surfs->numSurfs * sizeof(*surfs->surfs));
    surfs->next = xmodelSurfs;
    xmodelSurfs = surfs;
    file = (XModelSurfsFile_s *)alloc(sizeof(*file));
    file->data = surfs;
    file->free = XModelSurfsFree;
    file->name = source->name;
    return file;
}

/*
================
XModelSurfsPrecache

Load and cache a version 14 surface file using the supplied allocator.
An already cached file gets a new wrapper and surface pointer array.
================
*/
static XModelSurfsFile_s *XModelSurfsPrecache(const char *name, void *(*alloc)(unsigned int))
{
    char filename[1024];
    char *buf, *pos;
    int version, i;
    short numSurfs;
    XModelSurfsFile_s *file;
    XModelSurfs_s *surfs;
    XSurface_s *surface;

    file = (XModelSurfsFile_s *)FS_GetDataForFile("xmodelsurfs", name, "");
    if (!file) {
        Com_Printf("Cannot precache 'xmodelsurfs/%s'.\n", name);
        return 0;
    }
    if (file->data)
        return XModelSurfsCloneSurfs(file, alloc);
    sprintf(filename, "xmodelsurfs/%s", name);
    if (FS_ReadFile(filename, (void **)&buf) < 0) {
        Com_Error(1, "\025Cannot find 'xmodelsurfs/%s'.", name);
        return 0;
    }
    pos = buf;
    XMODEL_READ_SHORT(version, &pos);
    if (version != 14) {
        FS_FreeFile(buf);
        Com_Error(1, "\025xmodelsurfs '%s' out of date (version %d, expecting %d)", name, version, 14);
        return 0;
    }
    XMODEL_READ_SHORT(numSurfs, &pos);
    surfs = (XModelSurfs_s *)alloc(sizeof(*surfs));
    surfs->numSurfs = numSurfs;
    surfs->surfs = (XSurface_s **)alloc(numSurfs * sizeof(*surfs->surfs));
    surface = (XSurface_s *)alloc(numSurfs * sizeof(*surface));
    for (i = 0; i < numSurfs; i++) {
        surfs->surfs[i] = &surface[i];
        XModelReadSurface(surfs->surfs[i], &pos, alloc);
    }
    FS_FreeFile(buf);
    file->data = surfs;
    file->free = XModelSurfsFree;
    surfs->next = xmodelSurfs;
    xmodelSurfs = surfs;
    return file;
}


#define XMODEL_VECTOR_COPY(a, b) { \
    (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; \
}
#define XMODEL_VEC_SCALE(v, s, o) { \
    (o)[0] = (v)[0] * (s); (o)[1] = (v)[1] * (s); (o)[2] = (v)[2] * (s); \
}
#define XMODEL_VEC_SUB(a, b, c) { \
    (c)[0] = (a)[0] - (b)[0]; (c)[1] = (a)[1] - (b)[1]; (c)[2] = (a)[2] - (b)[2]; \
}

/*
================
XModelPartsPrecache

Load bone names, parent index deltas, bind quaternions and translations.
Compute a center and bounding sphere from each bone's bounds.
================
*/
XModelPartsFile_s *XModelPartsPrecache(const char *name, void *(*alloc)(unsigned int))
{
    char filename[1024];
    char *buf, *pos;
    int version, i, len, axis;
    short numChildBones, numBones;
    short numRootBones;
    XModelPartsFile_s *file;
    XModelPartsInfo *parts;
    XBoneNames_s *names;
    unsigned short *boneNames;
    XBoneHierarchy_s *hierarchy;
    XBoneInfo_s *boneInfo, *info;
    unsigned char *parentList;
    short (*quats)[4];
    float (*trans)[3];
    float delta[3];

    file = (XModelPartsFile_s *)FS_GetDataForFile("xmodelparts", name, "");
    if (!file) {
        if (xmodelOptimize)
            Com_Error(1, "\025Cannot precache 'xmodelparts/%s'.\nyou may need to get latest and run converter to fix", name);
        else
            Com_Printf("ERROR: Cannot precache 'xmodelparts/%s'", name);
        return 0;
    }
    if (file->data)
        return file;
    sprintf(filename, "xmodelparts/%s", name);
    if (FS_ReadFile(filename, (void **)&buf) < 0) {
        Com_Error(1, "\025Cannot find 'xmodelparts/%s'.", name);
        return 0;
    }
    pos = buf;
    XMODEL_READ_SHORT(version, &pos);
    if (version != 14) {
        FS_FreeFile(buf);
        Com_Error(1, "\025xmodelparts '%s' out of date (version %d, expecting %d)", name, version, 14);
        return 0;
    }
    XMODEL_READ_SHORT(numChildBones, &pos);
    XMODEL_READ_SHORT(numRootBones, &pos);
    numBones = numChildBones + numRootBones;
    names = (XBoneNames_s *)alloc(sizeof(short) + numBones * sizeof(unsigned short));
    names->size = numBones;
    boneNames = names->names;
    if (numBones >= 128) {
        Com_Error(1, "\025xmodel '%s' has more than %d bones", name, 127);
        return 0;
    }
    hierarchy = (XBoneHierarchy_s *)alloc(sizeof(*hierarchy) - 1 + numChildBones);
    hierarchy->names = names;
    parentList = hierarchy->parentList;
    /* Version 14 reserves 104 bytes plus 24 per child bone for pose data.
     * The packed quaternion and translation arrays use 20 bytes per child. */
    parts = (XModelPartsInfo *)alloc(104 + numChildBones * 24);
    parts->hierarchy = hierarchy;
    parts->quats = (short (*)[4])(parts + 1);
    parts->trans = numChildBones ? (float (*)[3])(parts->quats + numChildBones) : 0;
    parts->partClassification = (unsigned char *)alloc(numBones);
    parts->numRootBones = numRootBones;
    boneInfo = (XBoneInfo_s *)alloc(numBones * sizeof(*boneInfo));
    quats = parts->quats;
    trans = parts->trans;
    for (i = numRootBones; i < numBones; i++, quats++, trans++, parentList++) {
        int index = (unsigned char)*pos++;
        *parentList = i - index;
        XMODEL_VECTOR_COPY((const float *)pos, *trans);
        pos += sizeof(*trans);
        (*trans)[0] = LittleFloat((*trans)[0]);
        (*trans)[1] = LittleFloat((*trans)[1]);
        (*trans)[2] = LittleFloat((*trans)[2]);
        ReadQuat((short *)pos, *quats);
        pos += 3 * sizeof(short);
    }
    for (i = 0, info = boneInfo; i < numBones; i++, info++) {
        float *mins = info->bounds[0];
        float *maxs = info->bounds[1];
        float *offset = info->offset;
        len = strlen(pos) + 1;
        boneNames[i] = SL_GetStringOfLen(pos, 0, len, 10);
        pos += len;
        XMODEL_VECTOR_COPY((const float *)pos, info->bounds[0]);
        pos += sizeof(info->bounds[0]);
        info->bounds[0][0] = LittleFloat(info->bounds[0][0]);
        info->bounds[0][1] = LittleFloat(info->bounds[0][1]);
        info->bounds[0][2] = LittleFloat(info->bounds[0][2]);
        XMODEL_VECTOR_COPY((const float *)pos, info->bounds[1]);
        pos += sizeof(info->bounds[1]);
        info->bounds[1][0] = LittleFloat(info->bounds[1][0]);
        info->bounds[1][1] = LittleFloat(info->bounds[1][1]);
        info->bounds[1][2] = LittleFloat(info->bounds[1][2]);
        for (axis = 0; axis < 3; axis++)
            offset[axis] = mins[axis] + maxs[axis];
        XMODEL_VEC_SCALE(offset, 0.5f, offset);
        XMODEL_VEC_SUB(maxs, offset, delta);
        info->radiusSquared = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
    }
    memcpy(parts->partClassification, pos, numBones);
    FS_FreeFile(buf);
    parts->boneInfo = boneInfo;
    file->data = parts;
    file->free = XModelPartsFree;
    return file;
}


typedef struct XModelLodConfig_s {
    char filename[1024];
    float dist;
} XModelLodConfig_s;

typedef struct XModelConfig_s {
    XModelLodConfig_s lodInfo[3];
    float mins[3];
    float maxs[3];
    int collLod;
} XModelConfig_s;

/*
================
XModelLoadConfigFile

Read model bounds, three detail-level filenames and distances, and the
collision detail level. Return the next file position, or null for a bad version.
================
*/
static char *XModelLoadConfigFile(const char *name, char *pos, XModelConfig_s *config)
{
    int version, i;
    XMODEL_READ_SHORT(version, &pos);
    if (version != 14) {
        Com_Error(1, "\025xmodel '%s' out of date (version %d, expecting %d)", name, version, 14);
        return 0;
    }
    XMODEL_READ_VEC3(config->mins, &pos);
    XMODEL_READ_VEC3(config->maxs, &pos);
    for (i = 0; i < 3; i++) {
        XMODEL_READ_FLOAT(config->lodInfo[i].dist, &pos);
        strcpy(config->lodInfo[i].filename, pos);
        pos += strlen(pos) + 1;
    }
    config->collLod = LittleLong(*(int *)pos);
    return pos + 4;
}

/*
================
XModelLoadCollData

Read collision triangles, bone indices, bounds and flags into allocated surfaces.
Expand the bounds slightly, collect model contents and return the next file position.
================
*/
static char *XModelLoadCollData(char *pos, XModelInfo *model, void *(*alloc)(unsigned int))
{
    int i, j;
    XModelCollSurf_s *surf;
    XModelCollTri_s *tri;
    model->numCollSurfs = LittleLong(*(int *)pos);
    pos += 4;
    model->contents = 0;
    if (!model->numCollSurfs) {
        model->collSurfs = 0;
        return pos;
    }
    model->collSurfs = alloc(model->numCollSurfs * sizeof(XModelCollSurf_s));
    for (i = 0; i < model->numCollSurfs; i++) {
        surf = (XModelCollSurf_s *)model->collSurfs + i;
        surf->numCollTris = LittleLong(*(int *)pos);
        pos += 4;
        surf->collTris = (XModelCollTri_s *)alloc(surf->numCollTris * sizeof(XModelCollTri_s));
        for (j = 0; j < surf->numCollTris; j++) {
            tri = surf->collTris + j;
            XMODEL_READ_VEC3(tri->plane, &pos);
            XMODEL_READ_FLOAT(tri->plane[3], &pos);
            XMODEL_READ_VEC3(tri->svec, &pos);
            XMODEL_READ_FLOAT(tri->svec[3], &pos);
            XMODEL_READ_VEC3(tri->tvec, &pos);
            XMODEL_READ_FLOAT(tri->tvec[3], &pos);
        }
        /* Expand collision bounds to tolerate points on the surface. */
        surf->mins[0] = LittleFloat(*(float *)pos) - 0.001f;
        pos += 4;
        surf->mins[1] = LittleFloat(*(float *)pos) - 0.001f;
        pos += 4;
        surf->mins[2] = LittleFloat(*(float *)pos) - 0.001f;
        pos += 4;
        surf->maxs[0] = LittleFloat(*(float *)pos) + 0.001f;
        pos += 4;
        surf->maxs[1] = LittleFloat(*(float *)pos) + 0.001f;
        pos += 4;
        surf->maxs[2] = LittleFloat(*(float *)pos) + 0.001f;
        pos += 4;
        surf->boneIdx = LittleLong(*(int *)pos);
        pos += 4;
        surf->contents = LittleLong(*(int *)pos) & ~(CONTENTS_NONCOLLIDING | CONTENTS_AREAPORTAL | CONTENTS_TRANSLUCENT);
        pos += 4;
        surf->surfFlags = LittleLong(*(int *)pos);
        pos += 4;
        model->contents |= surf->contents;
    }
    return pos;
}

/*
================
XModelExists

Return whether the file cache contains an entry for the named model.
================
*/
qboolean XModelExists(const char *name)
{
    return (qboolean)(FS_GetDataForFile("xmodel", name, "") != 0);
}

/*
================
XModelPrecache

Cache the model configuration and collision data, then load its skeleton
and, when requested, its render surfaces. Missing parts use the default model.
================
*/
XModel_s *XModelPrecache(const char *name, int loadSurfaces, void *(*alloc)(unsigned int), void *(*allocTemp)(unsigned int))
{
    char filename[1024];
    XModelConfig_s config;
    int lengths[3];
    int size, i, j;
    char *buf, *pos, *strings;
    XModel_s *file;
    XModelInfo *model;

    file = (XModel_s *)FS_GetDataForFile("xmodel", name, "");
    if (!file)
        return 0;
    model = file->data;
    if (!model) {
        sprintf(filename, "xmodel/%s", name);
        if (FS_ReadFile(filename, (void **)&buf) < 0) {
            Com_Error(1, "\025Cannot find 'xmodel/%s'.", name);
            return 0;
        }
        pos = XModelLoadConfigFile(name, buf, &config);
        if (!pos) {
            FS_FreeFile(buf);
            return 0;
        }
        size = 0;
        for (i = 0; i < 3; i++) {
            lengths[i] = strlen(config.lodInfo[i].filename) + 1;
            size += lengths[i];
        }
        model = (XModelInfo *)alloc(sizeof(*model) + size);
        pos = XModelLoadCollData(pos, model, alloc);
        model->numLods = 0;
        strings = (char *)(model + 1);
        for (i = 0; i < 3; i++) {
            strcpy(strings, config.lodInfo[i].filename);
            model->lodInfo[i].filename = strings;
            if (*strings) {
                model->numLods++;
                model->lodInfo[i].numsurfs = *(short *)pos;
                pos += 2;
                model->lodInfo[i].surfNames = (unsigned short *)alloc(model->lodInfo[i].numsurfs * sizeof(unsigned short));
                for (j = 0; j < model->lodInfo[i].numsurfs; j++) {
                    const char *surfName;
                    surfName = pos;
                    pos += strlen(pos) + 1;
                    model->lodInfo[i].surfNames[j] = SL_GetString_(surfName, 0, 8);
                }
            } else {
                model->lodInfo[i].surfNames = 0;
            }
            model->lodInfo[i].dist = config.lodInfo[i].dist;
            strings += lengths[i];
        }
        FS_FreeFile(buf);
        model->mins[0] = config.mins[0];
        model->mins[1] = config.mins[1];
        model->mins[2] = config.mins[2];
        model->maxs[0] = config.maxs[0];
        model->maxs[1] = config.maxs[1];
        model->maxs[2] = config.maxs[2];
        model->collLod = config.collLod;
        file->data = model;
        file->free = XModelFree;
        model->lodInfo[0].surfs = 0;
        model->lodInfo[1].surfs = 0;
        model->lodInfo[2].surfs = 0;
    } else if (model == &g_default.modelInfo) {
        return file;
    }
    model->parts = XModelPartsPrecache(model->lodInfo[0].filename, alloc);
    if (model->parts) {
        if (!loadSurfaces)
            return file;
        for (i = 0; i < 3; i++) {
            if (!*model->lodInfo[i].filename)
                return file;
            model->lodInfo[i].surfs = XModelSurfsPrecache(model->lodInfo[i].filename, alloc);
            if (!model->lodInfo[i].surfs) {
                XModelFree(file);
                XModelCreateDefault(file);
                return file;
            }
        }
        return file;
    }
    XModelFree(file);
    XModelCreateDefault(file);
    return file;
}

/*
================
XModelClearData

Unlink surface sets in the released arena range and clear renderer pointers
into that range on the remaining surfaces.
================
*/
void XModelClearData(unsigned char *pLow, unsigned char *pHigh)
{
    XModelSurfs_s *model;
    XModelSurfs_s **ppPrevNext;
    XSurface_s *surface;
    int i;
    ppPrevNext = &xmodelSurfs;
    for (model = xmodelSurfs; model; model = model->next) {
        if ((unsigned char *)model >= pLow && (unsigned char *)model < pHigh) {
            *ppPrevNext = model->next;
        } else {
            for (i = 0; i < model->numSurfs; i++) {
                surface = model->surfs[i];
                if (surface) {
                    if (surface->renderData[1] >= (void *)pLow && surface->renderData[1] < (void *)pHigh)
                        surface->renderData[1] = 0;
                    if (surface->renderData[0] >= (void *)pLow && surface->renderData[0] < (void *)pHigh)
                        surface->renderData[0] = 0;
                }
            }
            ppPrevNext = &model->next;
        }
    }
}

/*
================
XModelNumBones

Return the number of bones in the model skeleton.
================
*/
int XModelNumBones(const XModel_s *model)
{
    return model->data->parts->data->hierarchy->names->size;
}

/*
================
XModelBoneNames

Return the array of interned names in skeleton bone order.
================
*/
unsigned short *XModelBoneNames(const XModel_s *model)
{
    return model->data->parts->data->hierarchy->names->names;
}

/*
================
XModelGetBoneIndex

Search bone names backward and return the index of the interned name.
Return -1 if it is absent.
================
*/
int XModelGetBoneIndex(const XModel_s *model, unsigned short name)
{
    unsigned short *boneNames;
    int i;
    boneNames = XModelBoneNames(model);
    i = XModelNumBones(model);
    while (--i >= 0)
        if (name == boneNames[i])
            break;
    return i;
}

/*
================
XModelGetBounds

Copy the model bounds into the supplied minimum and maximum vectors.
================
*/
void XModelGetBounds(const XModel_s *model, float *mins, float *maxs)
{
    const XModelInfo *info;
    info = model->data;
    mins[0] = info->mins[0];
    mins[1] = info->mins[1];
    mins[2] = info->mins[2];
    maxs[0] = info->maxs[0];
    maxs[1] = info->maxs[1];
    maxs[2] = info->maxs[2];
}

/*
================
XModelGetSurfaces

Return the surface count and expose the surface pointer array for a detail level.
================
*/
int XModelGetSurfaces(const XModel_s *model, XSurface_s ***surfaces, int lod)
{
    *surfaces = model->data->lodInfo[lod].surfs->data->surfs;
    return model->data->lodInfo[lod].surfs->data->numSurfs;
}

/*
================
XModelGetSurfaceName

Return a detail level's surface material name, or DEFAULT for an unnamed material.
================
*/
const char *XModelGetSurfaceName(const XModel_s *model, int subMatIndex, int lod)
{
    unsigned short name;
    name = model->data->lodInfo[lod].surfNames[subMatIndex];
    return name ? SL_ConvertToString(name) : "DEFAULT";
}

/*
================
XSurfaceCloneSurface

Copy a surface record and its texture coordinates using the supplied allocator.
Clear renderer pointers while sharing the remaining geometry arrays.
================
*/
XSurface_s *XSurfaceCloneSurface(const XSurface_s *surface, void *(*alloc)(unsigned int))
{
    XSurface_s *clone;
    clone = (XSurface_s *)alloc(sizeof(XSurface_s));
    *clone = *surface;
    clone->renderData[0] = 0;
    clone->renderData[1] = 0;
    clone->texCoords = (float (*)[2])alloc(clone->numVerts * sizeof(*clone->texCoords));
    Com_Memcpy(clone->texCoords, surface->texCoords, clone->numVerts * sizeof(*clone->texCoords));
    return clone;
}

/*
================
XSurfaceGetNumVerts

Return the number of vertices stored in the surface.
================
*/
int XSurfaceGetNumVerts(const XSurface_s *surface) { return surface->numVerts; }

/*
================
XSurfaceGetNumTris

Return the stored triangle count, including any degenerate padding triangle.
================
*/
int XSurfaceGetNumTris(const XSurface_s *surface) { return surface->numTris; }

/*
================
XSurfaceTileMode

Return the surface texture tiling mode.
================
*/
int XSurfaceTileMode(const XSurface_s *surface) { return surface->tileMode; }

/*
================
XSurfaceGetTris

Copy triangle indices into the output array, optionally adding a base vertex index.
The offset path processes pairs of triangles from the padded index array.
================
*/
void XSurfaceGetTris(const XSurface_s *surface, unsigned short *pIndexes, unsigned short base)
{
    if (base) {
        const unsigned int *src;
        unsigned int *dest;
        unsigned int offset;
        int count, i;
        /* Each group of three words contains two triangles. Add the base
         * to both packed indices in every word. */
        src = (const unsigned int *)surface->triIndexes;
        offset = base | (base << 16);
        dest = (unsigned int *)pIndexes;
        count = surface->numTris >> 1;
        do {
            for (i = 0; i < 3; i++)
                *dest++ = *src++ + offset;
        } while (--count);
    } else {
        memcpy(pIndexes, surface->triIndexes, surface->numTris * 3 * sizeof(unsigned short));
    }
}

/*
================
XSurfaceGetBlendInfoArray

Return the surface's rigid or blended vertex records.
================
*/
void *XSurfaceGetBlendInfoArray(const XSurface_s *surface) { return surface->blendInfo; }

/*
================
XSurfaceGetTexCoordArray

Return the surface's texture coordinate array.
================
*/
float (*XSurfaceGetTexCoordArray(const XSurface_s *surface))[2] { return surface->texCoords; }
/*
================
XSurfaceGetVertexInfoArray

Return the additional bone influence records used by blended vertices.
================
*/
void *XSurfaceGetVertexInfoArray(const XSurface_s *surface) { return surface->vertexInfo; }

/*
================
XSurfaceGetBoneIndex

Return the rigid surface's bone matrix byte offset, or -1 for a blended surface.
================
*/
int XSurfaceGetBoneIndex(const XSurface_s *surface) { return surface->boneIndex; }

/*
================
XSurfaceRemapTextureCoordinates

Replace each texture coordinate pair using selected source components,
then apply the supplied scale and offset.
================
*/
void XSurfaceRemapTextureCoordinates(XSurface_s *surf, const float *scale,
    const float *offset, int x, int y)
{
    float (*texCoords)[2];
    int count;
    texCoords = surf->texCoords;
    for (count = surf->numVerts; count; count--, texCoords++) {
        float s, t;
        s = (*texCoords)[x];
        t = (*texCoords)[y];
        (*texCoords)[0] = s * scale[0] + offset[0];
        (*texCoords)[1] = t * scale[1] + offset[1];
    }
}

/*
================
XModelGetName

Return the model file name.
================
*/
const char *XModelGetName(const XModel_s *model) { return model->name; }

/*
================
XModelGetContents

Return the combined collision contents of the model.
================
*/
int XModelGetContents(const XModel_s *model) { return model->data->contents; }

/*
================
DObjSkelMatrixTransformVector43

Transform a point by a skeleton matrix, including its translation.
================
*/
void DObjSkelMatrixTransformVector43(const float *in, const DObjSkelMat *matrix, float *out)
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
DObjSkelMatrixTransformVector43WeightedAdd

Transform a point by a bone matrix and add its weighted contribution
to the output position.
================
*/
void DObjSkelMatrixTransformVector43WeightedAdd(const float *in, float weight,
    const DObjSkelMat *matrix, float *out)
{
    out[0] += (in[0] * matrix->axis[0][0] + in[1] * matrix->axis[1][0] +
        in[2] * matrix->axis[2][0] + matrix->origin[0]) * weight;
    out[1] += (in[0] * matrix->axis[0][1] + in[1] * matrix->axis[1][1] +
        in[2] * matrix->axis[2][1] + matrix->origin[1]) * weight;
    out[2] += (in[0] * matrix->axis[0][2] + in[1] * matrix->axis[1][2] +
        in[2] * matrix->axis[2][2] + matrix->origin[2]) * weight;
}

/*
================
DObjSkelMatrixTransformVector

Rotate a direction by a skeleton matrix without applying translation.
================
*/
void DObjSkelMatrixTransformVector(const float *in, const DObjSkelMat *matrix, float *out)
{
    out[0] = in[0] * matrix->axis[0][0] + in[1] * matrix->axis[1][0] + in[2] * matrix->axis[2][0];
    out[1] = in[0] * matrix->axis[0][1] + in[1] * matrix->axis[1][1] + in[2] * matrix->axis[2][1];
    out[2] = in[0] * matrix->axis[0][2] + in[1] * matrix->axis[1][2] + in[2] * matrix->axis[2][2];
}

/* ReadBlend converts file bone indices to byte offsets in the pose matrix array. */
#define XMODEL_BONE_MATRIX(pose, byteOffset) ((const DObjSkelMat *)((const char *)(pose) + (byteOffset)))

/*
================
XSurfaceGetVerts

Transform rigid vertices with one matrix, or accumulate the primary and
additional bone influences for blended vertices. Texture coordinates are
copied unchanged; normals use the primary bone without blending.
================
*/
void XSurfaceGetVerts(const XSurface_s *surface, const DObjSkelMat *pose,
    float *verts, float *texCoords, float *normals)
{
    int i, j, boneOffset;
    const XBlendVertex_s *vertex;
    const XSimpleBlendInfo_s *blend;
    const DObjSkelMat *mat;

    vertex = (const XBlendVertex_s *)surface->blendInfo;
    boneOffset = surface->boneIndex;
    if (texCoords)
        memcpy(texCoords, surface->texCoords, surface->numVerts * 2 * sizeof(float));
    if (boneOffset != -1) {
        mat = XMODEL_BONE_MATRIX(pose, boneOffset);
        for (i = surface->numVerts; i; --i) {
            if (normals) {
                DObjSkelMatrixTransformVector(vertex->normal, mat, normals);
                normals += 3;
            }
            DObjSkelMatrixTransformVector43(((const XRigidVertex_s *)vertex)->xyz, mat, verts);
            vertex = (const XBlendVertex_s *)((const XRigidVertex_s *)vertex + 1);
            verts += 3;
        }
    } else {
        blend = (const XSimpleBlendInfo_s *)surface->vertexInfo;
        for (int count = surface->numVerts; count; count--, verts += 3) {
            if (normals) {
                mat = XMODEL_BONE_MATRIX(pose, vertex->boneOffset);
                DObjSkelMatrixTransformVector(vertex->normal, mat, normals);
                normals += 3;
            }
            mat = XMODEL_BONE_MATRIX(pose, vertex->boneOffset);
            float *point = verts;
            DObjSkelMatrixTransformVector43(vertex->xyz, mat, point);
            j = vertex->numBlends;
            if (!j) {
                vertex++;
            } else {
                const XWeightedVertex_s *weighted = (const XWeightedVertex_s *)vertex;
                point[0] *= weighted->weight;
                point[1] *= weighted->weight;
                point[2] *= weighted->weight;
                do {
                    --j;
                    DObjSkelMatrixTransformVector43WeightedAdd(blend->xyz, blend->weight,
                        XMODEL_BONE_MATRIX(pose, blend->boneOffset), point);
                    ++blend;
                } while (j);
                vertex = (const XBlendVertex_s *)(weighted + 1);
            }
        }
    }
}

/*
================
XModelGetNumLods

Return the number of configured model detail levels.
================
*/
int XModelGetNumLods(const XModel_s *model) { return model->data->numLods; }

/*
================
XModelGetCollLod

Return the model detail level selected for collision.
================
*/
int XModelGetCollLod(const XModel_s *model) { return model->data->collLod; }

/*
================
XModelSetTestLods

Set a test detail-level distance, clamping negative distances to zero.
Setting level zero also enables or disables the test thresholds.
================
*/
void XModelSetTestLods(int lodLevel, float dist)
{
    if (!lodLevel)
        xmodelTestLods = dist >= 0;
    if (dist < 0)
        dist = 0;
    xmodelTestLodInfo[lodLevel].dist = dist;
}

/*
================
XModelSetTestLodDist

Set a positive distance override for detail selection, or clear it otherwise.
================
*/
void XModelSetTestLodDist(float dist)
{
    xmodelTestLodDist = dist > 0 ? dist : 0;
}

/*
================
XModelGetLodForDist

Choose the first detail level whose distance threshold admits the model.
Apply active test overrides and return -1 when all levels are too distant.
================
*/
int XModelGetLodForDist(const XModel_s *model, float dist)
{
    const XModelLodInfo_s *lodInfo;
    int i, numLods;
    if (!xmodelTestLods)
        lodInfo = model->data->lodInfo;
    else
        lodInfo = xmodelTestLodInfo;
    if (xmodelTestLodDist != 0)
        dist = xmodelTestLodDist;
    numLods = model->data->numLods;
    for (i = 0; i < numLods; i++) {
        if (lodInfo[i].dist == 0 || dist < lodInfo[i].dist)
            return i;
    }
    return -1;
}


/*
================
XModelGetBasePose

Build the bind pose in two passes. Keep the accumulated bone quaternions
in the matrix storage until all children have read their parents, then
replace them with affine matrices and compose the bind translations.
================
*/
void XModelGetBasePose(const XModel_s *model, DObjSkelMat *mat)
{
    XModelPartsInfo *parts;
    const unsigned char *parents, *parentList;
    const short (*quats)[4];
    const float (*trans)[3];
    DObjSkelMat *pose;
    int numBones, i;
    float quat[4];

    parts = model->data->parts->data;
    parentList = parts->hierarchy->parentList;
    parents = parentList;
    numBones = parts->hierarchy->names->size;
    quats = parts->quats;
    trans = parts->trans;
    /* Accumulate quaternions first, then replace them with affine matrices. */
    pose = mat;
    for (i = parts->numRootBones; i; i--, pose++) {
        pose->axis[0][0] = 0;
        pose->axis[0][1] = 0;
        pose->axis[0][2] = 0;
        pose->axis[0][3] = 1;
    }
    for (i = numBones - parts->numRootBones; i; i--, quats++, pose++, parents++) {
        quat[0] = (*quats)[0] * (1.0f / 32767.0f);
        quat[1] = (*quats)[1] * (1.0f / 32767.0f);
        quat[2] = (*quats)[2] * (1.0f / 32767.0f);
        quat[3] = (*quats)[3] * (1.0f / 32767.0f);
        QuatMultiply(quat, pose[-*parents].axis[0], pose->axis[0]);
    }
    /* All parent quaternions have been consumed; the storage can now change. */
    for (i = parts->numRootBones; i; i--, mat++) {
        mat->axis[0][0] = 1;
        mat->axis[0][1] = 0;
        mat->axis[0][2] = 0;
        mat->axis[1][0] = 0;
        mat->axis[1][1] = 1;
        mat->axis[1][2] = 0;
        mat->axis[2][0] = 0;
        mat->axis[2][1] = 0;
        mat->axis[2][2] = 1;
        mat->origin[0] = mat->origin[1] = mat->origin[2] = 0;
    }
    parents = parentList;
    for (i = numBones - parts->numRootBones; i; i--, mat++, trans++, parents++) {
        ConvertQuatToMat43(mat->axis[0]);
        DObjSkelMatrixTransformVector43(*trans, mat - *parents, mat->origin);
    }
}

struct trace_s;
/* The light compiler omits model collision tracing. */
/*
================
XModelTraceLine

Return -1 without changing the trace result; model collision tracing
is unavailable in this tool.
================
*/
int XModelTraceLine(XModel_s *model, struct trace_s *results,
    struct DObjSkelMat *pose, float *localStart, float *localEnd, int contentmask)
{
    return -1;
}

/* The tools build has no model collision query implementation. */
/*
================
XModelGetStaticBounds

Return false without writing bounds; the static bounds query
is unavailable in this tool.
================
*/
qboolean XModelGetStaticBounds(const XModel_s *model, const float axis[3][3],
    float *mins, float *maxs)
{
    return qfalse;
}
