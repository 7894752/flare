# Reconstruction notes: what is proven and what is a choice

Byte identity of the compiled image proves the machine code, not the source text. Where several spellings compile to the same bytes, the
choice among them is not evidence about the original. This file says which parts of the source are proven, which are inferred, and which exist
only to make the old compiler produce the original code.

As of 2026-10-18, all 1,255 project functions pass complete-body checks and the normal strict standalone build, after the documented metadata stamp,
is **IDENTICAL** to the original: 577,536 bytes. The full hash and metadata policy are in
[Known differences](KNOWN_DIFFERENCES.md); section 11 describes the verification and published sources.

## 1. Evidence order for names

1. Strings in the binary (error messages and format strings name many functions: `Q3Map_ReadPortalFile`, `Light_Compile`, `ParseEpair`, ...).
2. The published Quake III Arena, Return to Castle Wolfenstein and Wolfenstein: Enemy Territory sources, and the q3map2/GtkRadiant tools.
3. Symbol tables and link maps of other retail builds of the same game family.
4. Otherwise a descriptive Quake-style name.

The shipped image supplies no RTTI or debug names for the light and geometry classes, so their class names and most method names belong to group 4.
The names that belong to group 4 or are otherwise inferred, with the behaviour that justifies each, are listed in section 12.

## 2. The light classes

The hierarchy is recovered from ten 24-slot vtables (no destructor slot), the constructor chains (each constructor writes the vptr of every base in
turn) and which slots two classes share:

```
Light (abstract)
 |- AreaLight
 |- DirectionalLight
 |    '- SunLight
 '- PointLight
      |- AreaCellLight
      |- LinearLight
      |- AdjustedPointLight
      '- SpotLight
           '- AdjustedSpotLight
```

The vtable contents, constructor writes, accessed member offsets and widths, shared method bodies, and light-type values written to the BSP are
established by the image. Together they support this hierarchy. The class and method names describe those roles; the image does not uniquely
establish their spelling, the C++ class keys or the types and purposes of unread members. The light code is compiled as ten objects, one class
each (see "Objects").

## 3. Shared headers

- `Vector3d`, `Plane`, `Bounds`, `Winding` and the colour classes are structs with the exact layouts the callers prove; `struct` is used everywhere (the
  class key cannot be seen in the binary).
- `Vector3d` has an empty default constructor, a three-argument constructor and a memberwise copy constructor; the memberwise copy is what makes by-value
  returns compile to per-component moves. Construction and arithmetic are inline in `vector.h`; `bounds.h`, `plane.h`, `winding.h` and `color.h` carry the other classes' inline members.
  Objects compiled with optimisation expand them, objects compiled without it call the first emitter's copy.
- `ColorRGB` is a constructor-free colour struct that carries the colour methods; `Color3f` derives from it and adds value constructors and assignment from `ColorRGB`. The option colours
  (`sunColor`, `sunDiffuseColor`, `ambientColor`, `minLight`) are plain `ColorRGB` fields in the options record. Constructor-free storage avoids
  unobserved color initialization. The options record and `OptionsParserState` account for that object's two empty initialization entries;
  the two lightmap accumulator values also use `ColorRGB` storage. The two-level color hierarchy and its names remain inferred.

## 4. Objects: how the original object boundaries were found

The original image contains 45 copies of the two doubles 0.001 and 0.00001. Their grouping with strings, constants and function references supports
the object map below. The reconstruction uses a common-header pair to reproduce those contributions. An aggregate and two separate scalar statics
can produce the same bytes, so the original declaration and exact header organization are not established by the repeated values alone.

- the ten light classes are ten separate objects (each copy is followed by that class's own constants and read by that class's methods only);
- four small source files that look separate (plane/bounds classification, 2D polygon splitting, tolerance tests, collinearity) all read the same copy, so
  they were one object; likewise the polygon pool and the polygon containment tests;
- four larger files each contain two objects (vertex groups, map/entity reading, vector pool and winding, polygon-file and portal-file reading);
- scene-tail diffuse-sunlight work and occluder-pair work are separate real-function objects. These supply the formerly missing two constant pairs
  and C++ objects without diagnostic duplicate declarations. Their exact historical boundaries and source basenames remain inferred.

Rich-header reconciliation (the Rich header lists the number of objects per tool): subtracting the C runtime's share (13 C++ and 143 C objects, measured on a
hybrid link), the project has 49 C++ and 68 C objects. The object re-cut above brings the C++ count to exactly 49. Objects without the common header and
compiled as C++: the animation/object code, the xmodel code, the memory tree, the string list, the parser and the command library. The stock JPEG library is
compiled as C like the rest of the IJG files.

## 5. Things that exist only to satisfy the compiler

These are choices, not recovered source. Each reproduces the original bytes; a cleaner spelling that does the same is welcome.

| place | what | why |
|---|---|---|
| `AreaCellLight::SelectFace` | `const Vector3d &lightOrigin = origin;`, a named `double distance` and a tangent value snapshot | preserves x87 load and operand order; an early pointer binding is also exact, so the reference spelling is not uniquely established |
| `MiscModel::CollectOccluders` | `ModelAsset *const &asset = model;` | reference to a member pointer declared before its use; changes the load order, adds no storage |
| `DObjCreate` | a small `XAnimBufferLayout` record for header size and stride of the animation buffers | groups two real layout facts; scalar-replaced, emits nothing |
| `ProcessLightmapMask` | an unused local `int j;` | /Od keeps every declared local in the frame; removing it changes the frame size |
| `ProcessPolygonOcclusion`, `RemoveCoveredOccluders` | early-rejection `goto`, multi-loop escape | preserves the observed branches; the exact historical source spelling is not established |
| `UnparseEntitiesWithOrigins` | `length` initialization and an `originCopy` scratch store | the initializer affects an address-expression register tie and the scratch store survives compilation; their original consumer or rationale is unknown |
| `FilterBrushIntoTree`, `TraceSegment_r` | recursion pragmas immediately above the functions | brush filtering uses `inline_depth(2)` and `inline_recursion(on)`; tracing uses `inline_recursion(on)`. They reproduce the observed recursive expansion; the original directive spelling and location are not established |
| `ColorRGB::operator float *` | defined inline where the lightmap code uses it, out of line in the shader object | the original has one out-of-line copy and the lightmap code expands it; the original header arrangement is unknown |
| `SelectNonAxialSplitPlane`, `SelectAxialSplitPlane` | a `SplitCounts` struct with an inline `Score()` | an inline member with a pointer `this` is evaluated "abs first", as the original; a plain expression is not |
| several `DObj`/`XModel` functions | statement order of independent loads | the order decides which register holds a loop index |
| `DObjCalcSkel` | complete animation/world record references in both second-pass root-control branches | consistent typed record access reproduces the final x87 operand roles; ordinary arrays and arithmetic remain |
| `GL_DrawPolygon`, `Polygon::WorldToTexture` | point/output references and const axis references around the member calls | these source views preserve the member expansion and operand order; exact spelling remains inferred |
| `CollectOccluders_r` | a loop counter that is initialised and incremented but never read | removing it shortens the function; like the unused `j` above, it reads as a leftover of earlier code that was never cleaned up |
| `ConvertSurface`, `ConvertBrushes`, `RebuildBsp`, `Q3Map_Write`, `Q3Map_BuildLight` | results, counts and defaults that are computed or set but not used afterwards | each one still emits code in the original; removing it changes the function. Their former consumers are unknown |
| `DObjTraceline`, `MakeTreePortals_r`, `PlugBspLeak` | labelled `goto` exits from nested loops | the original has two separate exits from the inner search; structured loops merge them. Same style as Q3's own `goto breakOut` |
| several geometry and light files | small inline member functions (`Vector3d` component and accumulation operators, some `Bounds` and `ColorRGB` members) written locally in the `.cpp` that uses them | the original emits each first copy at its definition point between ordinary functions; see section 8 |
| `XSurface` bone masks, `DObjCreate`, the hunk allocator | byte-pointer writes and offsets (`((unsigned char *)partBits)[i >> 3]`, the animation mapping buffer address, the aligned allocation prefix) | the original addresses these as bytes; word-sized or typed spellings change the code. The hunk prefix follows Q3's allocator |
| `PreviewView` | constructor-free `StoredVector3d` components, viewed through the `PreviewVector` macro | the view keeps vector operations without adding constructors to persistent camera storage; the storage type, access API and original declarations remain inferred |

Compiler options per object (optimised or not, exception handling on or off, security cookie on or off) are the result of testing which setting reproduces
each object; the table is in `build/sources.json`.

## 6. Inferred names that are not recovered symbols

Class and method names of the light hierarchy; `PolyMappingForward`, `PolyMappingInverse`, their `orientation` fields and the
`PolyInfo` members `lightmapToWorld`, `worldToLightmap` and `worldToTexture`; `reserved0C`/`reserved5C`, `flag53`;
`directionalPlaneEpsilon`, `minimumSpotCosine`, `rootHalf`; the colour structs' names; the helper names in the preview code (`WindowRect`, whose ctor and
conversion operator are inferred from two three-byte functions that cannot distinguish them); `SplitCounts`. Where the name appears in the binary as a string,
the string is the evidence.

## 7. Small emitted bodies

- Duplicate-looking small bodies (`Cross`, `LightmapToWorld`, `GetFlags`, the `Vector3d` operators) include retained inline copies. Identical code in
  distinct symbols remains separate with the original link settings.
- Three-byte `mov eax, ecx; ret` bodies are consistent with empty constructors or this-returning conversion operators, but those bytes alone do not
  identify which operation or class produced a particular body. `WindowRect` is a descriptive model of the two preview bodies.
- The vector-constructor iterator exists once (an unoptimised copy); other objects' copies fold into it at link time.

## 8. Inline members defined locally in source files

The linker keeps the first selected copy of each shared inline function, and VC7 emits that copy where the function is *defined* in the translation
unit (no compiler option changes this). In `flare.exe` those first copies sit between ordinary functions, right after the code that first uses them,
not at the top of the object: for example `Vector3d::operator[]` between two light methods, `ColorRGB::Luminance` in the light code, and
`Vector3d::operator*=` and `/=` between the sun light's methods. The sources therefore define these small inline members locally, in the `.cpp` file
at the place where they appear in the binary, the same way the code already defines other local inline helpers (`Plane::SetSignBits`, `FloorToInt`,
`Polygon::GetBackPlane`, `Bounds::AddBounds`). The result is that some short inline bodies (the `Vector3d` component, accumulation and
`LengthSquared` helpers, some `Bounds` and `ColorRGB` members) are written out in more than one source file. This duplication is deliberate: the
binary shows each object had its own definition at that position. The original's exact file organisation for these helpers is not known.

## 9. Storage and code that has no reader

- **Option record.** The command-line options are one record, `options`, of type `options_t`; both names are authentic in the successor tool's link map
  (a later build), so the 2003 record name and membership are an inference from that build. A normal 62-member record compiled with VC7 has size 0x428
  and every member offset reproduces the address the original uses. `OptionsParserState` (the eight bytes in front of it) and its member names are
  inferred: the bool at +4 is read as an "initialized" flag, while the unsigned word before it is never read in the shipped executable, so its role and
  type are not established.
- **Unreferenced storage.** Two data objects exist in the original that no code, `.data` or `.rdata` reference: a 128-byte object behind the preview
  window's GL context handle (`previewWindowInfo`) and a 4-byte object at the start of the preview state (`previewStateSerial`). Only size and position are
  known; the names are neutral placeholders.
- **Debug-only preview code.** The static `GL_DrawBoundsOutline` helper in the preview renderer reproduces the compile order of the original object: the first
  user of an array of a class type is compiled last, and an earlier user keeps the following functions in file order and fixes the import order of
  the OpenGL functions. The helper now draws the current leaf's bounds corners under `_DEBUG`. Release compilation omits that call and discards the
  helper's body, while its declaration retains the ordering effect. Both the helper and the debug call are inferred, not recovered historical source.

## 10. Limits of identity

There are no binary differences in the verified stamped image. Original local/type/header spellings and some source organization are not
uniquely recoverable from that identity. The documented source exceptions, unread storage and original sunlight boundary limitation are
part of the reconstruction's limits. See [Known differences](KNOWN_DIFFERENCES.md).

## 11. Evidence and verification

The reconstructed image is byte-identical to the 2003-10-17 Call of Duty light compiler after the metadata normalization described in
[Known differences](KNOWN_DIFFERENCES.md). All 1,255 project functions have complete-body byte and relocation checks. Library functions
are accounted for separately. The complete linked file, rather than the function count alone, establishes image identity.

### Sources

The original executable supplies instruction bytes, constants, strings, data references, imports, virtual tables and observable layout.
It supplies no original C/C++ source text. Error messages establish some function names; most local names and many type names remain descriptive.

Published source is used in this order:

1. id Software's [Quake III Arena](https://github.com/id-Software/Quake-III-Arena), including its original q3map tools and shared engine code.
2. The [Return to Castle Wolfenstein](https://github.com/id-Software/RTCW-SP) and
   [Wolfenstein: Enemy Territory](https://github.com/id-Software/Enemy-Territory) releases for related engine variants.
3. GtkRadiant/q3map2 for later tool algorithms and family conventions, with differences checked against the 2003 image.
4. Symbol tables and link maps from other retail builds of the game family. A name or record in a later build is supporting evidence;
   it does not by itself establish the declaration used in this executable.

The JPEG implementation follows the Independent JPEG Group's [6b distribution](https://ijg.org/files/jpegsrc.v6b.tar.gz).
The compression implementation follows [zlib 1.1.4](https://github.com/madler/zlib/tree/v1.1.4).
The unzip implementation follows the Quake-family minizip 0.15 code. Adaptations and retained notices are described in
[Third-party notices](../THIRD_PARTY_NOTICES.md); the included notices govern those components.

### Static verification

Each project function was compiled with Visual C++ 7.0, build 13.00.9466, using its translation unit's measured options. A body check compares
every instruction byte and binds each relocation to the referenced function, object, string or constant. Jump tables, alignment tails and
C++ exception-handling contributions are included where applicable. A byte match with an unbound relocation is insufficient.

The standalone build then compiles the complete manifest and links normally, with no unresolved or multiply defined symbols and no forced
link. The full-file comparison covers headers, code, read-only and writable data, imports, section layout, padding and metadata. The Rich
table comes from the toolchain and object set; it is not patched to obtain equality.

The link timestamp and the PDB GUID and age vary independently of the reconstructed source. `--identical` writes a separate copy with the
three recorded values. It does not substitute code or data from an original executable. The original file is used only for comparison.

To repeat the comparison with your own compiler installation and original file, run from the repository root:

```text
python -I build/build.py --vc7 <Vc7> --strict --identical --original <original-exe>
```

The build verifies its toolchain hashes and reports the resulting hash and file differences. No game data or original executable is
distributed, and this verification does not execute the resulting program.

### What remains inferred

Equality of the file does not uniquely identify source expressions, local names, class keys, header boundaries or source filenames.
Member offsets and access widths constrain a layout without always determining the original member type or purpose. Unread storage and
discarded functions provide still less evidence. In particular, the preview bounds helper and its debug-only call are inferred source
organization. The release build discards the helper's body; that image does not establish its historical existence.

The reconstructed types, retained source forms, inline-definition placement and unread storage are explained in
the sections above; section 12 records descriptive names and their evidence.
The original sunlight plane-buffer boundary limitation is preserved and disclosed in [Known differences](KNOWN_DIFFERENCES.md).

This is a static reconstruction and reproducible-build result. No runtime test coverage, modern compiler portability or correction of the
original tool's defects is claimed by byte identity.

## 12. Inferred names

These tables record descriptive source names and the evidence for their roles. A name from an executable string or a related published
source has stronger support than a name chosen to describe an observed operation. Member offsets and access widths can establish a layout
without establishing the original type, purpose or spelling. Equivalent names and declarations may produce the same compiled image.

The tables cover current release sources; excluded seed files and obsolete audit labels are omitted. Local and parameter names without stronger evidence are descriptive.
Overloaded constructors and operators use their C++ source spellings; separate address entries can describe different overloads or emitted copies.

### Object organization

| Name | Status | Evidence |
| --- | --- | --- |
| `occluderpairs.cpp` | inferred module name | Contains the existing FindOrCreateOccluderPair registry operation at 0x410360, split from occluder under the same decision. No matching original object name was found in the CoD5 map evidence. |
| `occluderpairs.h` / `OccluderPair` | inferred header/type names | The existing three-pointer record is shared between its unchanged global definitions in occluder and its allocator/search in occluderpairs. Field order and body bytes are preserved; source spelling is descriptive. |

### Geometry and preview names

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `PolyMappingForward, PolyMappingInverse` | `src/flare/geometry_types.h` | inferred | Aggregate names describe proved forward/inverse mapping layouts. The approved geometry pass replaces three cast views with real PolyInfo subobjects; names and aggregate shape remain inferred. |
| `PolyInfo::{lightmapToWorld, worldToLightmap, worldToTexture}` | `src/flare/geometry_types.h` | inferred | Real mapping subobjects at +0x50, +0xA0 and +0xE8. CreatePoly, affine solvers and projection consumers establish direction and layout; the original aggregate spelling is unavailable. |
| `PolyMappingInverse::{sAxis, tAxis, origin, orientation}` | `src/flare/geometry_types.h` | inferred | SolveLightmapMapping constructs p = s*sAxis + t*tAxis + origin and stores (sAxis cross tAxis) dot normal. Named vector members preserve the target's empty construction; a real vector array adds a constructor loop absent from AllocPolyInfo. |
| `PolyMappingForward::{uv, orientation}` | `src/flare/geometry_types.h` | inferred | UVMapping occupies the first 0x40 bytes; SolveTextureMapping stores (uv.s cross uv.t) dot normal at +0x40. This establishes the signed area-scale role formerly called extra. |
| `LightmapMappingMatches::mapping` | `src/flare/polygon.cpp` | inferred | The validator now takes the real inverse mapping by const reference and reads named members, replacing traversal beyond the first of three separate vectors. The complete target body and relocations remain exact after a checked COFF rebind. |
| `GL_WaitForStep` | `preview_step.cpp` | inferred | When preview enabled and step level threshold met, uploads lightmaps then draws frames until continue byte set |
| `GL_ClearStep` | `preview_step.cpp` | inferred | Clears five highlighted-object pointers read by preview BSP drawing |
| `GL_SetStepLevel` | `preview_step.cpp` | inferred | step level = %i string; clamps to -1..4 |
| `GL_IncreaseStepLevel` | `preview_step.cpp` | inferred | Inline step-level setter applied to incremented global, same diagnostic string and clamp |
| `GL_DecreaseStepLevel` | `preview_step.cpp` | inferred | Inline step-level setter applied to decremented global |
| `GL_Continue` | `preview_step.cpp` | inferred | Sets the byte polled by GL_WaitForStep |
| `GL_Step` | `preview_step.cpp` | inferred | Clears highlighted objects and tail-calls step wait when preview enabled |
| `GL_StepPolygon` | `preview_step.cpp` | inferred | Sets highlighted polygon pointer rendered by single polygon draw helper at 427B20 |
| `GL_StepPolygons` | `preview_step.cpp` | inferred | Sets linked polygon-list highlight rendered by following polygon next at offset 4C |
| `GL_StepPortal` | `preview_step.cpp` | inferred | Sets portal highlight pointer rendered by portal helper at 427C10 |
| `GL_StepNode` | `preview_step.cpp` | inferred | Sets node highlight pointer; preview renders its polygons at 54 and incident portal chain at 58 |
| `GL_StepPortalNode` | `preview_step.cpp` | inferred | Sets both node and portal highlight pointers then waits for continue |
| `GL_StepBrush` | `preview_step.cpp` | inferred | Sets highlighted brush pointer, later passed to face-loop renderer at 427E20 |
| `GL_CullFace` | `preview_state.cpp` | inferred | Cached cull-face mode, glCullFace and GL_CULL_FACE enable-disable |
| `GL_SetState` | `preview_state.cpp` | inferred | XOR state cache controls texture targets, client arrays, write masks, depth-stencil-alpha tests via named GL imports |
| `GL_StencilIncrement` | `preview_state.cpp` | inferred | GL stencil pass: state 2C4000, LESS, ALWAYS and GL_INCR, branch chooses front-back cull and depth pass-fail operation |
| `GL_StencilDecrement` | `preview_state.cpp` | inferred | Mirrored stencil pass uses GL_DECR and opposite front-back culling |
| `GL_StencilLitPass` | `preview_state.cpp` | inferred | Completes stencil volume rendering: state 2B4000, stencil EQUAL zero, depth EQUAL, front-face culling |
| `previewNumTextureUnits / previewBoundTextures / previewActiveTexture / previewClientTexture / previewCullFace / previewState` | `preview_state.cpp` | inferred | Measured GL state cache and target addresses; descriptive source names, unchanged. |
| `PreviewStep / previewStep / polygon / polygons / portal / node / brush` | `preview_step.cpp` | inferred | One five-pointer highlight record consumed by the preview drawing paths; type and member spellings are inferred. |
| `previewStepLevel / previewContinue` | `preview_step.cpp` | inferred | Clamped level and continue byte read by the wait loop; unchanged descriptive names. |
| `texCoordBit / targetShift` | `preview_state.cpp` | inferred | Separate live roles for the client-array mask and two-bit texture-target position; replaces the reused local bit. |
| `MAX_PREVIEW_TEXTURE_UNITS / GLSTATE_TEXTURE_TARGET_BITS / GLSTATE_TEXTURE_UNIT_MASK` | `preview_state.cpp` | inferred | Names inferred; values proved by the four-entry cache, two-bit shifts and field mask. |
| `GLSTATE_DEPTH_WRITE_SHIFT / GLSTATE_COLOR_WRITE_SHIFT / GLSTATE_ALPHA_WRITE_SHIFT / PREVIEW_STENCIL_MASK` | `preview_state.cpp` | inferred | Names inferred; values are the observed bit positions 15/16/17 and 8-bit stencil write mask. |
| `PREVIEW_STEP_MIN / PREVIEW_STEP_MAX` | `preview_step.cpp` | inferred | Names inferred; the exact clamp bounds -1 and 4 occur in the target and unchanged code. |
| `state / changed / unit / target / rgb / face / front / level` | `preview_state.cpp; preview_step.cpp` | inferred | Ordinary semantic parameter/local names, inferred. No numbered or machine-shaped names remain. |

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `AddLeafLight` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AddLeafLight::{ intensity, leaf, light, link, minIntensity, newLight, numEntries, prev }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `WarnTooManyLeafLights` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `WarnTooManyLeafLights::{ intensity, intensityText, leaf, light, message, origin, originText }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `AddLightAtPoint` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AddLightAtPoint::{ distance, light, node, point }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `FilterLightPolygon` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `FilterLightPolygon::{ light, poly }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `FilterLightPolygon_r` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `FilterLightPolygon_r::{ backPolygon, front, iSideFlags, light, node, poly, splitPlane }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `FindLeafLight` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `FindLeafLight::{ intensity, leaf, light, link }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `AddLightIndices` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AddLightIndices::{ count, i, indices, j }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `LoadBspForLighting` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `LoadBspForLighting::{ name }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `GetEntityLighting` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `GetEntityLighting::{ color, coords, ds, dt, end, entity, i, lightmap, s0, t0, text, trace }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `WriteBspLighting` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `WriteBspLighting::{ entity, i, name, text }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `BuildLeafLightIndices` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `BuildLeafLightIndices::{ clusterIndex, indices, j, lightLink, numIndexes }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `FindLightIndex` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `FindLightIndex::{ i, light }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `AddBspLight` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AddBspLight::{ light }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `CompareLightIndices` | `src/flare/lightindices.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `CompareLightIndices::{ a, b, l1, l2 }` | `src/flare/lightindices.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `lights` | `src/flare/lightindices.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `lastDiscardedIntensity` | `src/flare/lightindices.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `POLYFLAG_VISUAL_ONLY` | `src/flare/lightindices.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_MODEL_SURFACE` | `src/flare/lightindices.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_PORTAL` | `src/flare/lightindices.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `MAX_LEAF_LIGHTS` | `src/flare/lightindices.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `LIGHTMAP_SIZE` | `src/flare/lightindices.cpp` | inferred | Q3 code/renderer/tr_bsp.c:134 names LIGHTMAP_SIZE=128. Flare uses 512, so this establishes only the naming convention, not a recovered flare macro. Historical spelling remains inferred. |
| `SIDE_FRONT` | `src/flare/lightindices.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `SIDE_BACK` | `src/flare/lightindices.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `SIDE_CROSS` | `src/flare/lightindices.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `LeafLight::{next, intensity, light}` | `src/flare/lightindices.cpp` | inferred | Linked list entry storing one light and its maximum contribution to a BSP leaf. |
| `MAX_MAP_LIGHTS, MAX_MAP_LIGHT_INDEXES` | `src/flare/lightindices.cpp` | qfiles.h | Existing format constants from code/qcommon/qfiles.h, reused without changing the bounds. |
| `SplitPolygon` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SplitPolygonEpsilon` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `ClipPolygonFront` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `ClipPolygonBack` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PolygonPlaneSide` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PolygonPlaneSide::{ back, distance, front, i, plane, poly }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PolygonArea2D` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PolygonArea2D::{ area, count, i, points }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PolygonCentroid2D` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PolygonCentroid2D::{ area, centroid, count, i, points, x, y }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `SplitPolygon2D` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SplitPolygon2D::{ axis, back, backdist, count, distFront, fraction, front, i, j, mid, numBackVertices, numFrontVertices, outBackCount, outFrontCount, points, sides, split }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `SubdividePolygon2D` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `NearlyEqual` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `NearlyEqual::{ a, b }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PlaneFromPoints` | `src/flare/polygonclip.cpp` | Q3 donor | Quake-III-Arena/q3map/light.c or common/mathlib.c has the same named geometric operation; flare implementation differs. |
| `PlaneFromPoints::{ a, b, c, distance, normal }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PerpendicularVector3d` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PerpendicularVector3d::{ normal, point }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Vector3d::Normalize` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Vector3d::Normalize::{ inverseLength, length }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `BuildOrthonormalBasis` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `BuildOrthonormalBasis::{ normal, right, up }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PointsAreCollinear` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PointsAreCollinear::{ aLengthSquared, cross, edge2, length2, p1, p2, p3, v1 }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PointToPolygonFormFactor` | `src/flare/polygonclip.cpp` | Q3 donor | Quake-III-Arena/q3map/light.c or common/mathlib.c has the same named geometric operation; flare implementation differs. |
| `PointToPolygonFormFactor::{ count, currentDir, dotProduct, edgeNormal, k, multiplier, normal, normals, point, pointIndex, points, sinAngleSquared, solidAngle }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Vector3d::Normalized` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Vector3d::Normalized::{ length }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `MaxDouble` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `MaxDouble::{ a, b }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Vector3d::NormalizeLength` | `src/flare/polygonclip.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Vector3d::NormalizeLength::{ inverse, length }` | `src/flare/polygonclip.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `collinearityEpsilon` | `src/flare/polygonclip.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `MAX_VERTS_PER_POLY` | `src/flare/polygonclip.cpp` | flare diagnostic | SplitPolygon2D reports MAX_VERTS_PER_POLY; arrays and limit check establish 512. |
| `Q_PI` | `src/flare/polygonclip.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `SIDE_FRONT` | `src/flare/polygonclip.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `SIDE_BACK` | `src/flare/polygonclip.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `SIDE_ON` | `src/flare/polygonclip.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `SIDE_CROSS` | `src/flare/polygonclip.cpp` | Q3 donor convention | Q3 mathlib/polylib naming; the flare values and literal precision are retained. |
| `Vector2Copy` | `src/flare/polygonclip.cpp` | ET/q3map2 donor macro | Enemy-Territory/src/game/q_shared.h:652 and GtkRadiant/tools/quake3/q3map2/light_trace.c:45 define Vector2Copy(a,b) as the same two component assignments used here. Exact macro spelling/shape has donor evidence. |
| `XModelAlloc::{ size }` | `src/flare/model.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Bounds::Clear` | `src/flare/model.cpp` | inferred | Q3 code/splines/math_vector.h:430 has Bounds::Clear with the same empty-bounds min/max role, but uses +/-99999 rather than flare FLT_MAX. Donor convention supports this descriptive spelling; historical flare spelling remains inferred. |
| `LoadModel` | `src/flare/model.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `LoadModel::{ i, lod, map, mats, model, name, numSurfaces, obj, objModel, partBits, shadow, shadowName, skin, surface, surfaceInfo, xmodel }` | `src/flare/model.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `FindModel` | `src/flare/model.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `FindModel::{ map, model, name }` | `src/flare/model.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `MiscModel::ShouldIgnore` | `src/flare/model.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `MiscModel::ShouldIgnore::{ entityNum }` | `src/flare/model.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `MiscModel` | `src/flare/model.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `MiscModel::{ axis, flags, i, j, map, name, origin, v }` | `src/flare/model.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `MiscModel::CollectOccluders` | `src/flare/model.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `MiscModel::CollectOccluders::{ asset, clipped, entityNum, firstPoly, head, i, i0, i1, i2, j, n, numPolys, occluder, points, poly, st, surfaceFlags, tail }` | `src/flare/model.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `modelList` | `src/flare/model.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `MAX_POLY_OCCLUDERS` | `src/flare/model.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_NOSHADOW` | `src/flare/model.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_ALPHASHADOW` | `src/flare/model.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_MODEL_SURFACE` | `src/flare/model.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `GetToken` | `src/flare/parse.cpp` | Q3 token-parser lineage + CoD1 Mac xSYM | The Mac xSYM names table contains .GetToken__FPPcbb; Q3 common/scriplib.c names the quoted/comment-aware token reader GetToken. Supports the operation/name family; the Mac argument list differs from flare and is not copied. |
| `GetToken::{ crossLine, data, length, maxChars, success, text, token }` | `src/flare/parse.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `SkipLine` | `src/flare/parse.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SkipLine::{ data, text }` | `src/flare/parse.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `GetLine` | `src/flare/parse.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `GetLine::{ data, length, line, maxChars, success, text }` | `src/flare/parse.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Bounds::Clear` | `src/flare/polygon.cpp` | inferred | Q3 code/splines/math_vector.h:430 has Bounds::Clear with the same empty-bounds min/max role, but uses +/-99999 rather than flare FLT_MAX. Donor convention supports this descriptive spelling; historical flare spelling remains inferred. |
| `InitPolygonPool` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `InitPolygonPool::{ maxCount }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `AllocPolygon` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AllocPolygon::{ poly }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `CreatePoly` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AllocPolyInfo` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AllocPolyInfo::{ info }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `SolveTextureMapping` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SolveTextureMapping::{ coord, determinant, i, i0, i1, i2, inverse, j, major, mapping, normal, points, st }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `TextureMappingMatches` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `TextureMappingMatches::{ i, mapping, numPoints, points, st }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `AbsoluteEqual` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AbsoluteEqual::{ a, b }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `TextureMappingMatchesAbsolute` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `TextureMappingMatchesAbsolute::{ i, mapping, numPoints, points, st }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `SolveLightmapMapping` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SolveLightmapMapping::{ determinant, i0, i1, i2, inverse, mapping, normal, points, st }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `LightmapMappingMatches` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `LightmapMappingMatches::{ axes, i, numPoints, points, st }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `MappingVectorsEqual` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `MappingVectorsEqual::{ a, b, delta }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `PolygonVerticesEqual` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `PolygonVerticesEqual::{ a, b, delta }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `AllocSplitPolygon` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `AllocSplitPolygon::{ numPoints, poly, source }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `RetainPolyInfo` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `RetainPolyInfo::{ info }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `OffsetPolygon` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `OffsetPolygon::{ delta, distance, i, poly, source }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `CopyPolyInfo` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `CopyPolyInfo::{ copy, info }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `CopyPolygon` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `CopyPolygon::{ poly, source }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::UpdateLightmapBounds` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::UpdateLightmapBounds::{ centroid, i, max, mins, st }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::Free` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `ReleasePolyInfo` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `ReleasePolyInfo::{ info }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `MakePolygonPoolPermanent` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SavePolygonPool` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `RestorePolygonPool` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SaveTemporaryPolygons` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `SaveTemporaryPolygons::{ flags }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `RestoreTemporaryPolygons` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `RestoreTemporaryPolygons::{ firstPoly, i, last, numVectors, poly }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `CompactLastPolygon` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `CompactLastPolygon::{ iVert, last, numPoints, oldNumVerts, prevPoly }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `FinalizePolygons` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `FinalizePolygons::{ i }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Vector3d::Normalized` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Vector3d::Normalized::{ length }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Vector3d::NormalizeLength` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Vector3d::NormalizeLength::{ inverse, length }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `ComparePolygons` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `ComparePolygons::{ a, b, difference, index1, index2, lightmapDifference, polygon1, polygon2 }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::InterpolateNormal` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Vector3d::Length` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::IntersectsWinding` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::IntersectsWinding::{ basePlane, clipped, edge, i, plane, previous, winding, windingBounds }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::ContainsWinding` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::ContainsWinding::{ basePlane, edge, i, plane, planeSide, previous, winding, windingBounds }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::TrimPortalWinding` | `src/flare/polygon.cpp` | flare diagnostic | The allocation-failure string names TrimPortalWinding. |
| `Polygon::TrimPortalWinding::{ back, basePlane, classifications, edge, fragments, frontWinding, j, numCrossings, originalWinding, planes, prev, winding, windingBounds }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::PointOnPolygon` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::PointOnPolygon::{ basePlane, distance, point }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::ContainsPoint` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::ContainsPoint::{ basePlane, distance, edge, edgeNormal, epsilon, i, point, previous }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Bounds::Contains` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Bounds::Contains::{ point }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `Polygon::IntersectSegment` | `src/flare/polygon.cpp` | inferred | Descriptive name for the recovered operation; original spelling is not established. |
| `Polygon::IntersectSegment::{ d1, d2, end, midpoint, polyPlane, start }` | `src/flare/polygon.cpp` | inferred | Parameters and local spellings describe the argument, geometry, parsing, pool or loop role; no original local symbols survive. |
| `numPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `peakPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `numPermanentPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `numBspPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `polygonPoolPermanent` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `freePolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `polygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `maxPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `savedNumPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `temporaryNumPolygons` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `numPermanentVectors` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `savedNumVectors` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `temporaryNumVectors` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `temporaryPolygonFlags` | `src/flare/polygon.cpp` | inferred | Name describes the recovered allocation, pool counter/state, list or mathematical value; address/role evidence does not prove spelling. |
| `MAX_VERTS_PER_POLY` | `src/flare/polygon.cpp` | flare diagnostic | SplitPolygon2D reports MAX_VERTS_PER_POLY; arrays and limit check establish 512. |
| `LIGHTMAP_SIZE` | `src/flare/polygon.cpp` | inferred | Q3 code/renderer/tr_bsp.c:134 names LIGHTMAP_SIZE=128. Flare uses 512, so this establishes only the naming convention, not a recovered flare macro. Historical spelling remains inferred. |
| `MAPPING_DETERMINANT_EPSILON` | `src/flare/polygon.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `TEXTURE_COORD_EPSILON` | `src/flare/polygon.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `MIN_POLYGON_EDGE_LENGTH_SQUARED` | `src/flare/polygon.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_PORTAL` | `src/flare/polygon.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_NOSHADOW` | `src/flare/polygon.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |
| `POLYFLAG_VISUAL_ONLY` | `src/flare/polygon.cpp` | inferred | Descriptive constant name; the existing literal and its type are preserved. |

### Light and geometry names

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `ComparePolygons` | `polygon.cpp` | inferred | 0x0041A250; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::InterpolateNormal` | `polygon.cpp` | inferred | 0x0041A350; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Vector3d::Length` | `polygon.cpp` | inferred | 0x0041B560; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::IntersectsWinding` | `polygon.cpp` | inferred | 0x0041B5A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::ContainsWinding` | `polygon.cpp` | inferred | 0x0041BA30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::TrimPortalWinding` | `polygon.cpp` | flare diagnostic | 0x0041BD40; The original string at 0x477100 names TrimPortalWinding; 0x41C2C4 pushes it and 0x41C2C9 calls Q3Map_Error inside this function. Corrects the duplicate row; the earlier polygon row already recorded this evidence. |
| `Polygon::PointOnPolygon` | `polygon.cpp` | inferred | 0x0041C5C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::ContainsPoint` | `polygon.cpp` | inferred | 0x0041C640; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Bounds::Contains` | `polygon.cpp` | inferred | 0x0041C8C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::IntersectSegment` | `polygon.cpp` | inferred | 0x0041C950; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_Verbose` | `print.cpp` | inferred | 0x0041CC90; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_Printf` | `print.cpp` | inferred | 0x0041CCD0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_Warning` | `print.cpp` | inferred | 0x0041CD00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_VError` | `print.cpp` | inferred | 0x0041CD40; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_Error` | `print.cpp` | inferred | 0x0041CD70; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeEntity` | `entities.cpp` | inferred | 0x0041CFD0; Q3/RTCW/ET hits refer to the game entity release/unlink lifecycle (G_FreeEntity), not this map-entity key/value allocation cleanup; no name promotion. |
| `BuildEntity` | `entities.cpp` | inferred | 0x0041D200; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `VectorForKey` | `entities.cpp` | inferred | 0x0041D980; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `IntForKey` | `entities.cpp` | Q3 donor | 0x0041E690; Quake-III-Arena/q3radiant/ENTITY.CPP:146 has IntForKey(entity,key), returning atoi(ValueForKey(...)); flare adds a null-value check. Supports the same entity-key operation/name, not recovery of original local identifiers. |
| `Q3Map_BuildMiscModel` | `entities.cpp` | inferred | 0x0041E740; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LoadXModelSurfaces` | `entities.cpp` | inferred | 0x0041EED0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `EntityAffectsLighting` | `entities.cpp` | inferred | 0x0041FF80; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_ReadEntityText` | `entities.cpp` | inferred | 0x004201A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SurfaceFlags` | `entities.cpp` | inferred | 0x00420240; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_Free` | `bspio.cpp` | inferred | 0x00420680; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LoadMapShaders` | `bspio.cpp` | inferred | 0x00420690; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LoadModelSurfaces` | `bspio.cpp` | inferred | 0x004207D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LoadSurface` | `bspio.cpp` | inferred | 0x00420850; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ConvertSurface` | `bspio.cpp` | inferred | 0x004208D0; q3map2 convert_ase uses this generic name for exporting a surface to an ASE file. Flare builds its own polygon from disk BSP data; the same word is insufficient identity evidence. |
| `ConvertBrushes` | `bspio.cpp` | inferred | 0x00421030; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ConvertEntityBrushes` | `bspio.cpp` | inferred | 0x004210D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `RebuildBsp` | `bspio.cpp` | inferred | 0x00421490; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ConvertLightmaps` | `bspio.cpp` | inferred | 0x00421660; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_Write` | `bspio.cpp` | inferred | 0x00421720; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::ToBytes` | `bspio.cpp` | inferred | 0x004217E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `MaxInt` | `bspio.cpp` | inferred | 0x004218F0; Donor hits are FreeType comments about saturated integer limits, not this maximum helper; no function-name evidence. |
| `MinInt` | `bspio.cpp` | inferred | 0x00421920; Donor hits are FreeType comments about saturated integer limits, not this minimum helper; no function-name evidence. |
| `ScanShaderDefinitions` | `shaders.cpp` | inferred | 0x00421950; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseStage` | `shaders.cpp` | Q3 donor + CoD1 Mac xSYM | 0x00421AA0; Quake-III-Arena/code/renderer/tr_shader.c:587 names the brace-delimited map/blendFunc/alphaFunc parser ParseStage; the Mac xSYM names table contains .ParseStage. Flare has the same stage-level role under ParseShader; its different signature is retained. Canonically renamed and re-proved. |
| `ShaderFileNameCompare` | `shaders.cpp` | inferred | 0x00421E80; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::operator*=` | `shaders.cpp` | inferred | 0x004220E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::operator/=` | `shaders.cpp` | inferred | 0x00422100; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseShader` | `shaders.cpp` | Q3 donor + CoD1 Mac xSYM | 0x004224E0; Quake-III-Arena/code/renderer/tr_shader.c:1397 names ParseShader, with the same outer-brace/nested-stage parsing role; CoD1 Mac xSYM names includes .ParseShader. Flare has additional tool-only fields and a different signature. |
| `Q3Map_LoadShader` | `shaders.cpp` | inferred | 0x004229D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Q3Map_RequestShader` | `shaders.cpp` | inferred | 0x00422C80; Operation is established by accepted target code; no original spelling is recovered from the searched references. |

### Engine names

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `Q_isnamechar` | q_shared.c | inferred | Describes the accepted alphanumeric/underscore/hyphen predicate; original spelling unproven. |
| `MemoryNode.prev` | scr_memorytree.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `MemoryNode.next` | scr_memorytree.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `MemoryNode.data` | scr_memorytree.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `mt_size` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `mt_allocCount` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `mt_bitCount` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `mt_head` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `mt_numBits` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `mt_leftBits` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `mt_allocBuckets` | scr_memorytree.cpp | inferred | Counter is incremented by 1 << size and printed as allocated buckets. Replaces the misleading mt_allocBytes; unchanged target storage 0x64B348. |
| `mt_logBits` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `scrStringInited` | scr_memorytree.cpp | inferred | Allocator storage/table role inferred from target accesses and MT_Init/MT_DumpTree; original identifier is unknown. |
| `MT_NODE_SIZE` | scr_memorytree.cpp | inferred | Names the observed allocator node count, eight-byte granularity, size-class bound, byte-table size, or allocation limit; original macro spelling is unknown. |
| `MT_BIT_TABLE_SIZE` | scr_memorytree.cpp | inferred | Names the observed allocator node count, eight-byte granularity, size-class bound, byte-table size, or allocation limit; original macro spelling is unknown. |
| `MT_MAX_ALLOC_SIZE` | scr_memorytree.cpp | inferred | Names the observed allocator node count, eight-byte granularity, size-class bound, byte-table size, or allocation limit; original macro spelling is unknown. |
| `RefString.refCount` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `RefString.byteLen` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `RefString.user` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `RefString.str` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `HashEntry` | scr_stringlist.cpp | inferred | Script-string record or hash-chain role inferred from target accesses; original identifier is unknown. |
| `HashEntry.next` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `HashEntry.status` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `HashEntry.u.prev` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `HashEntry.u.str` | scr_stringlist.cpp | inferred | Member role inferred from target loads/stores and layout. This is not a recovered source identifier. |
| `lastHash` | scr_stringlist.cpp | inferred | Script-string record or hash-chain role inferred from target accesses; original identifier is unknown. |
| `HASH_TABLE_SIZE` | `scr_stringlist.cpp` | inferred | The Q3 cvar/filesystem hash-table spelling is from a different subsystem and table size. It does not recover the string-list table macro. |
| `HASH_ENTRY_MOVABLE` | scr_stringlist.cpp | inferred | Names the observed 16384-entry table, eight-byte allocator stride, or two-bit entry status/mask; original macro/enum spelling is unknown. |
| `HASH_ENTRY_HEAD` | scr_stringlist.cpp | inferred | Names the observed 16384-entry table, eight-byte allocator stride, or two-bit entry status/mask; original macro/enum spelling is unknown. |
| `freeIndex` | scr_stringlist.cpp | inferred | Index from hashTable[0], the free-list head; replaces an unnecessary copied HashEntry. |
| `cvar_hashTable` | cvar.c | inferred | Descriptive local name for the cvar hash buckets; donor supplies the algorithm, not this spelling. |
| `parseMark_t` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `com_parseSeparators` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `parseInfo_stack` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `parseInfo_level` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `com_lineBuffer` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `lastTokenPos` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `MAX_PARSE_SESSIONS` | q_parse.cpp | inferred | Parser mark/session/token storage role inferred from target accesses and parse-session strings; original identifier is unknown. |
| `csField_t` | q_shared.c | inferred | Config-field descriptor type inferred from the target table layout and dispatcher; original type spelling is unknown. |
| `csFieldType_t` | q_shared.c | inferred | Config-field descriptor type inferred from the target table layout and dispatcher; original type spelling is unknown. |
| `CS_FIELD_VECTOR` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_STRING1024` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_STRING64` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_STRING256` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_INT` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_QBOOLEAN` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_FLOAT` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_MILLISECONDS` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `CS_FIELD_COUNT` | q_shared.c | inferred | Descriptive built-in config-field tag for the observed conversion/size; count sentinel is eight. Original enum spelling is unknown. |
| `magnitude` | q_shared.c | inferred | Local absolute lean fraction produced by Q_fabs; descriptive local spelling. |

### Additional source names

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `GetPickedLightOrigin` | `lightscene.cpp` | inferred | 0x0040A280; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `GetPickedLightDirection` | `lightscene.cpp` | inferred | 0x0040A290; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FloodBspFromLights` | `lightscene.cpp` | inferred | 0x0040A2A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DrawSceneLights` | `lightscene.cpp` | inferred | 0x0040A300; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PickSceneLight` | `lightscene.cpp` | inferred | 0x0040A340; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PointLight::PointLight` | `lightscene.cpp` | inferred | 0x0040BC00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `IsAreaLightVisible_r` | `lightscene.cpp` | inferred | 0x0040BC60; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `TraceDiffuseSample` | `lightscene.cpp` | inferred | 0x0040C0C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DiffuseSampleCallback` | `lightscene.cpp` | inferred | 0x0040C560; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Vector3d::Cross` | `lightscene.cpp` | inferred | 0x0040CFC0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::LightmapToWorld` | `lightscene.cpp` | inferred | 0x0040D050; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CellDiffuseSampleCallback` | `lightscene.cpp` | inferred | 0x0040D0E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `GenerateDiffuseSamples` | `lightscene.cpp` | inferred | 0x0040D9D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Vector3d::NormalizeLength` | `lightscene.cpp` | inferred | 0x0040E2C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SampleDiffuseLighting` | `lightscene.cpp` | inferred | 0x0040E350; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DiffuseLightCallback` | `lightscene.cpp` | inferred | 0x0040E4D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Polygon::WorldToLightmap` | `lightscene.cpp` | inferred | 0x0040E700; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterDiffuseSunlight_r` | `lightscene.cpp` | inferred | 0x0040E7A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CalculateSceneLighting` | `lightscene.cpp` | inferred | 0x0040E8F0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PrepareLightOccluders` | `lightscene.cpp` | inferred | 0x0040EB10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CollectOccluders_r` | `lightscene.cpp` | inferred | 0x0040EC30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ProcessPolygonOcclusion` | `lightscene.cpp` | inferred | 0x0040EF30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LightPolygonOccluders_r` | `lightscene.cpp` | inferred | 0x0040F320; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PushAlphaShadowPoly` | `lightscene.cpp` | inferred | 0x0040F4E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PopAlphaShadowPoly` | `lightscene.cpp` | inferred | 0x0040F530; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LightPolygon` | `lightscene.cpp` | inferred | 0x0040F550; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AccumulateLightmapLighting` | `lightscene.cpp` | inferred | 0x0040F6C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilteredLightmapCallback` | `lightscene.cpp` | inferred | 0x0040F800; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `HasAlphaShadowPolys` | `lightscene.cpp` | inferred | 0x0040FAF0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PolygonInFrontOfPlane` | `lightscene.cpp` | inferred | 0x0040FB00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CellDiffuseLightCallback` | `lightscene.cpp` | inferred | 0x0040FB80; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CellHasDiffuseSunlight` | `lightscene.cpp` | inferred | 0x0040FE00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AllocOccluderSidePlanes` | `occluder.cpp` | inferred | 0x0040FEA0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeOccluderSidePlanes` | `occluder.cpp` | inferred | 0x0040FEE0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AllocOccluder` | `occluder.cpp` | inferred | 0x0040FEF0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeOccluder` | `occluder.cpp` | inferred | 0x0040FF30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AddOccluder` | `occluder.cpp` | inferred | 0x0040FF60; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ClearOccluderPools` | `occluder.cpp` | inferred | 0x0040FFF0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeOccluderPools` | `occluder.cpp` | inferred | 0x00410010; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `UnlinkOccluder` | `occluder.cpp` | inferred | 0x00410070; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CompareOccluderArea` | `occluder.cpp` | inferred | 0x004100B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `RemoveCoveredOccluders` | `occluder.cpp` | inferred | 0x00410110; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FindOrCreateOccluderPair` | `occluder.cpp` | inferred | 0x00410360; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BuildLightmapMask` | `lightmap.cpp` | inferred | 0x004103B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BuildLightmapMask_r` | `lightmap.cpp` | inferred | 0x00410430; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BuildPolygonLightmapMask` | `lightmap.cpp` | inferred | 0x004104A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LightmapMaskCallback` | `lightmap.cpp` | inferred | 0x004106F0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ProcessLightmapMask` | `lightmap.cpp` | inferred | 0x00410840; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `InitAmbientLightmaps` | `lightmap.cpp` | inferred | 0x00410B60; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AllocLightmapSamples` | `lightmap.cpp` | inferred | 0x00410C10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `GenerateLightmapAreas` | `lightmap.cpp` | inferred | 0x00410DA0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AccumulateLightmapArea` | `lightmap.cpp` | inferred | 0x00410F10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `InitLightmapColors` | `lightmap.cpp` | inferred | 0x00410F90; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FinalizeLightmaps` | `lightmap.cpp` | inferred | 0x00411110; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `RemoveLmapAreaInSolid_r` | `lightmap.cpp` | inferred | 0x00411240; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FinalizeLightmapAreas` | `lightmap.cpp` | inferred | 0x004113C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DebugLightmapAreas` | `lightmap.cpp` | inferred | 0x00411540; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::FromBytes` | `lightmap.cpp` | inferred | 0x00411820; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LightmapSample::LightmapSample` | `lightmap.cpp` | inferred | 0x00411890; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CountLitVertices` | `litvertex.cpp` | inferred | 0x004118A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `LitVertex::LitVertex` | `litvertex.cpp` | inferred | 0x004118C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AllocLitVertices` | `litvertex.cpp` | inferred | 0x004118D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CountVertexGroup` | `vertexgroup.cpp` | inferred | 0x00411980; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ForEachVertexGroup` | `vertexgroup.cpp` | inferred | 0x00411990; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Vector3d::operator[]` | `vertexgroup.cpp` | inferred | 0x004119D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Bounds::Clear` | `vertexgroup.cpp` | inferred | 0x004119E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CreateVertexGroup` | `vertexgroup.cpp` | inferred | 0x00411A00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `VertexGroup::ClearLighting` | `vertexgroup.cpp` | inferred | 0x00411C20; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ClearVertexGroupLighting` | `vertexgroup.cpp` | inferred | 0x00411C50; Operation is established by accepted target code; no original spelling is recovered from the searched references. |

### Storage and helper names

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `PlaneSideForBrush` | `brush.cpp` | inferred | 0x00401000; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeBrushPool` | `brush.cpp` | inferred | 0x00401160; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Plane::SetSignBits` | `brush.cpp` | inferred | 0x00401180; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `RoundToInt` | `brush.cpp` | inferred | 0x004011D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FloorToInt` | `brush.cpp` | inferred | 0x004011F0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Winding::Winding` | `brush.cpp` | inferred | 0x00401210; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Winding::~Winding` | `brush.cpp` | inferred | 0x00401220; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Winding::NumPoints` | `brush.cpp` | inferred | 0x00401240; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Winding::operator[]` | `brush.cpp` | inferred | 0x00401250; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Winding::operator=` | `brush.cpp` | inferred | 0x00401270; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BrushFace::~BrushFace` | `brush.cpp` | inferred | 0x00401280; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Plane::Plane` | `brush.cpp` | inferred | 0x004012A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BrushFace::operator=` | `brush.cpp` | inferred | 0x004012B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Vector3d::Snap` | `brush.cpp` | inferred | 0x004012E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BrushFace::BrushFace` | `brush.cpp` | inferred | 0x00401420; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeTempBrush` | `brush.cpp` | inferred | 0x00401550; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CopyTempBrush` | `brush.cpp` | inferred | 0x004015A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterBrushIntoTree` | `brush.cpp` | inferred | 0x00401C50; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterBrushesIntoTree` | `brush.cpp` | inferred | 0x00401ED0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CreateBrush` | `brush.cpp` | inferred | 0x00402040; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FloodBspFromPoint` | `bsp.cpp` | inferred | 0x00402240; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AddLeakPoint` | `bsp.cpp` | inferred | 0x00402290; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `RecordBspLeak` | `bsp.cpp` | inferred | 0x004023A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SelectSplitPlane` | `bsp.cpp` | inferred | 0x00402B90; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SelectNonAxialSplitPlane` | `bsp.cpp` | inferred | 0x00402C00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SelectAxialSplitPlane` | `bsp.cpp` | inferred | 0x00402D00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CompareFloat` | `bsp.cpp` | inferred | 0x00403160; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ComputeBspBounds` | `bsp.cpp` | inferred | 0x004031C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CountBspPolygons` | `bsp.cpp` | inferred | 0x004032D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Bounds::AddPoint` | `bsp.cpp` | inferred | 0x00403340; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Bounds::AddBounds` | `bsp.cpp` | inferred | 0x00403410; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `BuildPortals` | `bsp.cpp` | inferred | 0x004034E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Portal::~Portal` | `bsp.cpp` | inferred | 0x00403B00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Portal::operator=` | `bsp.cpp` | inferred | 0x00404060; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ClipBspPortals_r` | `bsp.cpp` | inferred | 0x00404490; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ClipPortalByPolygons` | `bsp.cpp` | inferred | 0x004045A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `CopyPortal` | `bsp.cpp` | inferred | 0x004047C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PlugBspLeak` | `bsp.cpp` | inferred | 0x004048C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FreeBspLeakPortal` | `bsp.cpp` | inferred | 0x00404A80; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ClearBspFlooded` | `bsp.cpp` | inferred | 0x00404AE0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PointOutsideBsp` | `bsp.cpp` | inferred | 0x00404B20; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::Normalize` | `flare.cpp` | inferred | 0x00404E50; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `MaxFloat` | `flare.cpp` | inferred | 0x00404ED0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Help` | `options.cpp` | inferred | 0x00404F00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Verbose` | `options.cpp` | inferred | 0x00404F10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Quiet` | `options.cpp` | inferred | 0x00404F30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Warn` | `options.cpp` | inferred | 0x00404F50; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseIntOption` | `options.cpp` | inferred | 0x00404F70; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Exec` | `options.cpp` | inferred | 0x00405000; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_BaseDir` | `options.cpp` | inferred | 0x00405040; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseStringOption` | `options.cpp` | inferred | 0x00405060; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Fast` | `options.cpp` | inferred | 0x004050D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SetFastOptions` | `options.cpp` | inferred | 0x004050E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Extra` | `options.cpp` | inferred | 0x00405190; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SetExtraOptions` | `options.cpp` | inferred | 0x004051A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MaxPolys` | `options.cpp` | inferred | 0x00405260; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MaxVerts` | `options.cpp` | inferred | 0x00405290; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MaxBrushes` | `options.cpp` | inferred | 0x004052C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MaxOccluders` | `options.cpp` | inferred | 0x004052F0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Visual` | `options.cpp` | inferred | 0x00405320; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoVisualOnlyPolys` | `options.cpp` | inferred | 0x00405340; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Width` | `options.cpp` | inferred | 0x00405360; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Height` | `options.cpp` | inferred | 0x00405390; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_Fullscreen` | `options.cpp` | inferred | 0x004053C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_PicMip` | `options.cpp` | inferred | 0x004053E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_DebugLmapAreas` | `options.cpp` | inferred | 0x00405400; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_DebugLmapBleeding` | `options.cpp` | inferred | 0x00405430; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_CorrectSolid` | `options.cpp` | inferred | 0x00405460; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoCorrectSolid` | `options.cpp` | inferred | 0x00405480; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_DiffuseSun` | `options.cpp` | inferred | 0x004054A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoDiffuseSun` | `options.cpp` | inferred | 0x004054C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SampleDiffuseSun` | `options.cpp` | inferred | 0x004054E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_FastDiffuseSun` | `options.cpp` | inferred | 0x00405500; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_ExtraDiffuseSun` | `options.cpp` | inferred | 0x00405520; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_AreaDiffuseSun` | `options.cpp` | inferred | 0x00405540; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_BackSplash` | `options.cpp` | inferred | 0x00405560; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoSmoothNormals` | `options.cpp` | inferred | 0x00405580; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_LeafLights` | `options.cpp` | inferred | 0x004055A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoLeafLights` | `options.cpp` | inferred | 0x004055C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_ModelShadow` | `options.cpp` | inferred | 0x004055E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoModelShadow` | `options.cpp` | inferred | 0x00405600; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_WorldModels` | `options.cpp` | inferred | 0x00405620; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_LocalModels` | `options.cpp` | inferred | 0x00405640; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_ModelAlphaShadow` | `options.cpp` | inferred | 0x00405660; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoModelAlphaShadow` | `options.cpp` | inferred | 0x00405680; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MergeOccluders` | `options.cpp` | inferred | 0x004056A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_NoMergeOccluders` | `options.cpp` | inferred | 0x004056C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_FastAlphaCheck` | `options.cpp` | inferred | 0x004056E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_ExtraAlphaCheck` | `options.cpp` | inferred | 0x00405700; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MaxAlphaPixels` | `options.cpp` | inferred | 0x00405720; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SplitCost` | `options.cpp` | inferred | 0x00405750; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_AxialThreshold` | `options.cpp` | inferred | 0x00405780; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SunColor` | `options.cpp` | inferred | 0x004057B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseVectorOption` | `options.cpp` | inferred | 0x00405800; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SunLight` | `options.cpp` | inferred | 0x00405870; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseFloatOption` | `options.cpp` | inferred | 0x004058A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SunDiffuseColor` | `options.cpp` | inferred | 0x00405950; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_DiffuseFraction` | `options.cpp` | inferred | 0x004059A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SunDirection` | `options.cpp` | inferred | 0x004059D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_MinLight` | `options.cpp` | inferred | 0x00405A40; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SunDiffuseSamples` | `options.cpp` | inferred | 0x00405A90; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_SunDiffuseStep` | `options.cpp` | inferred | 0x00405AB0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_AreaLightStep` | `options.cpp` | inferred | 0x00405AE0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Option_DumpOptions` | `options.cpp` | inferred | 0x00405B10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PrintBoolOption` | `options.cpp` | inferred | 0x00405DE0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PrintIntOption` | `options.cpp` | inferred | 0x00405E20; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PrintFloatOption` | `options.cpp` | inferred | 0x00405E40; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `PrintUsage` | `options.cpp` | inferred | 0x00405E60; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ExecuteOptions` | `options.cpp` | inferred | 0x00405ED0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ParseOptions` | `options.cpp` | inferred | 0x00405F10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `InitOptions` | `options.cpp` | inferred | 0x00405FF0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Preview_CvarFloat` | `config.cpp` | inferred | 0x004066C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FindFloatSetting` | `config.cpp` | inferred | 0x00406700; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Preview_SetCvarFloat` | `config.cpp` | inferred | 0x00406750; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Preview_CvarInteger` | `config.cpp` | inferred | 0x004067E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FindIntSetting` | `config.cpp` | inferred | 0x00406820; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Preview_SetCvarInteger` | `config.cpp` | inferred | 0x00406870; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Bind` | `config.cpp` | inferred | 0x004068E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Set` | `config.cpp` | inferred | 0x004069B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Toggle` | `config.cpp` | inferred | 0x00406BD0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Inc` | `config.cpp` | inferred | 0x00406D10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `AddSetting` | `config.cpp` | inferred | 0x00406D30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Dec` | `config.cpp` | inferred | 0x00406E90; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Add` | `config.cpp` | inferred | 0x00406EB0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Quit` | `config.cpp` | inferred | 0x00406F50; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Options` | `config.cpp` | inferred | 0x00406F70; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Exec` | `config.cpp` | inferred | 0x004070B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveLeftDown` | `config.cpp` | inferred | 0x00407120; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveLeftUp` | `config.cpp` | inferred | 0x00407140; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveRightDown` | `config.cpp` | inferred | 0x00407160; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveRightUp` | `config.cpp` | inferred | 0x00407180; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveUpDown` | `config.cpp` | inferred | 0x004071A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveUpUp` | `config.cpp` | inferred | 0x004071C0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveDownDown` | `config.cpp` | inferred | 0x004071E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_MoveDownUp` | `config.cpp` | inferred | 0x00407200; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_ForwardDown` | `config.cpp` | inferred | 0x00407220; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_ForwardUp` | `config.cpp` | inferred | 0x00407240; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_BackDown` | `config.cpp` | inferred | 0x00407260; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_BackUp` | `config.cpp` | inferred | 0x00407280; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_JumpToLight` | `config.cpp` | inferred | 0x004072A0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_LmapFilter` | `config.cpp` | inferred | 0x004072D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Overbright` | `config.cpp` | inferred | 0x004072E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_ToggleBreakpoint` | `config.cpp` | inferred | 0x004072F0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_ResetBreakpoints` | `config.cpp` | inferred | 0x00407300; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_StepIn` | `config.cpp` | inferred | 0x00407310; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_StepOut` | `config.cpp` | inferred | 0x00407320; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Cmd_Step` | `config.cpp` | inferred | 0x00407330; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ExecConfigFile` | `config.cpp` | inferred | 0x00407340; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ExecuteConfigLine` | `config.cpp` | inferred | 0x004074D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ComputePrincipalAxes` | `covariance.cpp` | inferred | 0x004076D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `SwapInt` | `covariance.cpp` | inferred | 0x00408150; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ExpandBgr24` | `image.cpp` | inferred | 0x00408180; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ExpandBgra32` | `image.cpp` | inferred | 0x004081D0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Color3f::Color3f` | `image.cpp` | inferred | 0x00408530; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Color3f::Color3f` | `image.cpp` | inferred | 0x00408540; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Color3f::Color3f` | `image.cpp` | inferred | 0x00408560; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::operator*` | `image.cpp` | inferred | 0x00408580; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::operator*` | `image.cpp` | inferred | 0x004085B0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::operator/` | `image.cpp` | inferred | 0x004085E0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `ColorRGB::Clear` | `image.cpp` | inferred | 0x00408610; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `RoundToInt` | `image.cpp` | inferred | 0x00408620; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FloorToInt` | `image.cpp` | inferred | 0x00408650; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Rgba8::Rgba8` | `image.cpp` | inferred | 0x00408680; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DecodeDxtColor` | `image.cpp` | inferred | 0x00408690; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DecodeDxt3` | `image.cpp` | inferred | 0x00408E00; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `DecodeDxt5` | `image.cpp` | inferred | 0x00408FD0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `Img_LoadDds` | `image.cpp` | inferred | 0x00409410; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTextureAlphaBlend` | `image.cpp` | inferred | 0x00409750; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTextureAlphaZero` | `image.cpp` | inferred | 0x00409870; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTextureAlphaLess128` | `image.cpp` | inferred | 0x00409940; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTextureAlphaAtLeast128` | `image.cpp` | inferred | 0x00409A10; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTextureCentroid` | `image.cpp` | inferred | 0x00409AE0; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTexturePolygon` | `image.cpp` | inferred | 0x00409C30; Operation is established by accepted target code; no original spelling is recovered from the searched references. |
| `FilterTexture` | `image.cpp` | inferred | 0x0040A130; Operation is established by accepted target code; no original spelling is recovered from the searched references. |

### Names and evidence

| Symbol | TU | Evidence | Reason |
| --- | --- | --- | --- |
| `previewWindowInfo` (`char[128]`) | `src/flare/preview_window.cpp` | inferred storage | 128 unreferenced bytes directly behind `previewGLContext` (0x636588..0x636608); no code, .data or .rdata reference exists |
| `previewStateSerial` (`int`) | `src/flare/preview_state.cpp` | inferred storage | 4 unreferenced bytes at 0x636608, the first variable of the state object, before `previewNumTextureUnits` (0x63660C) |

| Name | TU | Evidence and limits |
| --- | --- | --- |
| `diffusesunlight.cpp` | `diffusesunlight.cpp` | Descriptive module name for diffuse-sunlight queries; separate object/data contribution is established, historical basename and exact cut are inferred. |
| `OptionsParserState::reserved` | `options.cpp` | Unread first word in the eight-byte parser state; only the following initialized byte's role is established. |
| `options_t`, `options` | `options.cpp` | Record/type names occur in a later retail tool map; the 2003 record grouping and membership remain inferred. |
| `status_next` | `scr_stringlist.cpp` | Packed status/link member; successor assertion text supports this spelling and role, without recovering the 2003 declaration. |
| `prev` | `scr_stringlist.cpp` | Descriptive predecessor index used by the string-table walks. |
| `IMAGE_ALPHA_LESS_128`, `IMAGE_ALPHA_AT_LEAST_128` | `image.cpp` | Descriptive mode names for the stored alpha thresholds; transmission callbacks use the complementary rejection predicates. |
| `imageType`, `textureFilterImage` | `image.cpp` | Descriptive image-header snapshot and shared transmission-filter image state. |
| `GL_DrawBoundsOutline` | `preview_render.cpp` | Inferred helper and debug-only current-leaf call. Release compilation discards its body; its earlier class-array declaration affects function/import order. |
| `ColorRGB`, `Color3f` | `math_types.h` | Inferred constructor-free RGB storage/base and derived value class; layouts/operations constrain these roles without recovering the original declarations. |
| `PreviewView`, `StoredVector3d`, `PreviewVector` | `preview_view.h` | Inferred camera record, constructor-free vector storage and vector-access macro; original API/type spelling is unknown. |
| `MEMORY_NODE_COUNT`, `MEMORY_NODE_BITS` | `scr_memorytree.cpp` | Successor assertions support the node-count and size-class spellings; the 2003 declaration text remains inferred. |
| `MT_NODE_SIZE`, `HASH_STAT_MASK`, `HASH_STAT_MOVABLE`, `HASH_STAT_HEAD` | `scr_stringlist.cpp` | Successor assertions support these string-handle/status spellings and roles; the 2003 declaration text remains inferred. |
| `scr_memorytree.cpp`, `scr_stringlist.cpp`, `com_files.c`, `win_common.c` | `scr_memorytree.cpp` | Current module basenames have successor link-map evidence for their corresponding allocator, strings, filesystem and platform APIs; exact historical 2003 basenames are not established. |
| `DObjSkelMat`, `XModelInfo`, `XModelPartsInfo` | `xmodel.h; xmodel_records.h` | Assertion strings in the original related CoDRadiant tool support these skeleton-matrix and model/parts record names. Their complete 2003 declarations and every member spelling are not established by those strings alone. |

| Scope / old spelling | Applied spelling | Exact assertion evidence (original file and line) |
| --- | --- | --- |
| Hierarchy field `boneNames` | `names` | Same dobj.cpp:266 expression. Only the hierarchy member changes; DObj's own boneNames array retains its distinct role. |
| Bone-name record field `numBones` | `size` | Same dobj.cpp:266 expression; following code reads the short at offset zero. |
| LOD record field `numSurfs` | `numsurfs` | xmodel.cpp:1351: `(unsigned) subMatIndex < ((XModelInfo *) model->data.data)->lodInfo[lod].numsurfs` |
| Surface field `tris` | `triIndexes` | xmodel.cpp:1389: `(((int)surface->triIndexes) & 3) == 0` |

| Scope / old spelling | Applied spelling | Exact assertion evidence (original file and line) |
| --- | --- | --- |
| Strip record field `verts` | `stripIndexes` | xmodel.cpp:394: `indexes == strips.stripIndexes + neededIndexes` |
| DObjCreateDuplicateParts buffer `duplicates` | `duplicateParts` | dobj.cpp:339/342: `duplicateParts[len]` |
| DObjCreateDuplicateParts length `size` | `len` | dobj.cpp:356: `len >= (DOBJ_MAX_PART_BITS * sizeof(int))` |
| DObjCreateDuplicateParts offset `boneOffset` | `boneIndex` | dobj.cpp:332: `boneIndex + localBoneIndex + 1 < 256` |
| DObjCreateDuplicateParts index `j` | `localBoneIndex` | Same dobj.cpp:332 expression. |
| DObjCreateDuplicateParts lookup result `boneIndex` | `parentIndex` | dobj.cpp:324: `(parentIndex >= 0)`; line334: `parentIndex < boneIndex + localBoneIndex` |
| DObjGetHierarchyBits local index `localIndex` | `localBoneIndex` | dobj.cpp:439: `localBoneIndex >= 0` |
| Both hierarchy functions pair cursor `duplicates` | `pos` | dobj.cpp:464/589: `*pos` |
| DObjCompleteHierarchyBits parent `parentIndex` | `newBoneIndex` | dobj.cpp:599: `(unsigned) newBoneIndex < obj->numBones` |
| CalcSkel animation cursor `mat` | `childMat` | Same dobj.cpp:786, plus line764 quaternion and line765 translation NaN checks. |
| CalcSkel attachment `parent` | `modelParent` | dobj.cpp:819: `modelParent < boneIndex` |
| CalcSkel parent deltas `parents` | `parentList` | dobj.cpp:881: `((char *) skel->skelPartBits)[(boneIndex - (*parentList)) >> 3] & (1 << ((boneIndex - (*parentList)) & 7))` |
| CalcSkel duplicate cursor `duplicates` | `pos` | dobj.cpp:759: `boneIndex != (*pos) - 1`; line951: `!(*pos)` |
| CalcSkel duplicate-copy local `sourceMat` | `duplicateMat` | Inferred disambiguation for the old nested copy pointer, allowing the directly recovered base name sourceMat without shadowing it. No historical spelling claimed. |
| DObjCreate input `models` | `dobjModels` | dobj.cpp:1247: `dobjModels` |
| DObjGetSurfaceName parameter `surfaceIndex` | `subMatIndex` | dobj.cpp:1642: `(unsigned) subMatIndex < ((XModelInfo *) obj->models[modelIndex]->data.data)->lodInfo[lods[modelIndex]].numsurfs` |
| DObjTraceline matrix `matrix` | `boneMatrix` | dobj.cpp:1982: `!IS_NAN((boneMatrix->origin)[0]) && !IS_NAN((boneMatrix->origin)[1]) && !IS_NAN((boneMatrix->origin)[2])` |
| DObjTraceline combined bone `boneIndex` | `globalBoneIndex` | dobj.cpp:1974: `((char *) skel->skelPartBits)[globalBoneIndex >> 3] & (1 << (globalBoneIndex & 7))` |
| DObjTraceline plane delta `distance` | `dist` | dobj.cpp:2046/2060: `dist > 0`, `dist < 0` |
| XModelSurfsPrecache, XModelPartsPrecache, XModelPrecache file storage `buffer` | `buf` | xmodel.cpp:704/803/1114: `!buf`; lines709/808: `buf` |
| XModelPartsPrecache parent cursor `parents` | `parentList` | xmodel.cpp:868: `i - index == *parentList` |
| XModelPartsPrecache parent index `parentIndex` | `index` | xmodel.cpp:866: `index < i`, plus line868. |

| Scope / old spelling | Applied spelling | Exact assertion evidence (original file and line) |
| --- | --- | --- |
| XModelClearData list cursor `modelSurfs`, link `prev` | `model`, `ppPrevNext` | xmodel.cpp:1237: `*ppPrevNext == model` |
| XModelGetSurfaceName parameter `surfaceIndex` | `subMatIndex` | xmodel.cpp:1351 expression above. |
| XSurfaceGetTris destination `tris` | `pIndexes` | xmodel.cpp:1390: `(((int)pIndexes) & 3) == 0` |
| XSurfaceRemapTextureCoordinates parameter `surface` | `surf` | xmodel.cpp:1445: `surf` |

| Scope / old spelling | Applied spelling | Exact assertion evidence (original file and line) |
| --- | --- | --- |
| XModelSetTestLods parameter `lod` | `lodLevel` | xmodel.cpp:1633: `(unsigned) lodLevel < MAX_LODS`; surrounding code compares with three before testing for level zero and writing the selected test-LOD distance. |

| Source and function | Previous name | Current name | 2003 assertion evidence |
| --- | --- | --- | --- |
| `scr_memorytree.cpp`, `file scope` | `mt_nodes` | `nodes` | `((MemoryNode *) p - nodes >= 0 && (MemoryNode *) p - nodes < MEMORY_NODE_COUNT)` |
| `scr_memorytree.cpp`, `MT_AddMemoryNode` | `parent` | `parentNode` | `node == *parentNode; newNode != node; score != newScore; newNode != nodeNum` |
| `scr_memorytree.cpp`, `MT_AddMemoryNode` | `current` | `node` | `node == *parentNode; newNode != node; score != newScore; newNode != nodeNum` |
| `scr_memorytree.cpp`, `MT_AddMemoryNode` | `nodeNum` | `newNode` | `node == *parentNode; newNode != node; score != newScore; newNode != nodeNum` |
| `scr_memorytree.cpp`, `MT_AddMemoryNode` | `midpoint` | `nodeNum` | `node == *parentNode; newNode != node; score != newScore; newNode != nodeNum` |
| `scr_memorytree.cpp`, `MT_AddMemoryNode` | `score` | `newScore` | `node == *parentNode; newNode != node; score != newScore; newNode != nodeNum` |
| `scr_memorytree.cpp`, `MT_RemoveMemoryNode` | `parent` | `parentNode` | `node == *parentNode (same tree-link role); oldNode` |
| `scr_memorytree.cpp`, `MT_RemoveMemoryNode` | `current` | `oldNode` | `node == *parentNode (same tree-link role); oldNode` |
| `scr_memorytree.cpp`, `MT_RemoveHeadMemoryNode` | `parent` | `parentNode` | `node == *parentNode (same tree-link role); oldNode` |
| `scr_memorytree.cpp`, `MT_RemoveHeadMemoryNode` | `current` | `oldNode` | `node == *parentNode (same tree-link role); oldNode` |
| `scr_memorytree.cpp`, `MT_FreeIndex` | `buddyBit` | `lowBit` | `nodeNum == (nodeNum & ~(lowBit - 1))` |
| `scr_memorytree.cpp`, `MT_Free` | `ptr` | `p` | `((MemoryNode *) p - nodes >= 0 && (MemoryNode *) p - nodes < MEMORY_NODE_COUNT)` |
| `scr_stringlist.cpp`, `file scope` | `refString` | `refStr` | `refStr->refCount; (entry->status_next & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `file scope` | `HASH_ENTRY_FREE` | `HASH_STAT_FREE` | `refStr->refCount; (entry->status_next & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_FindStringOfLen` | `head` | `entry` | `newEntry != entry; (newEntry->status_next & HASH_STAT_MASK) == HASH_STAT_MOVABLE` |
| `scr_stringlist.cpp`, `SL_FindStringOfLen` | `entry` | `newEntry` | `newEntry != entry; (newEntry->status_next & HASH_STAT_MASK) == HASH_STAT_MOVABLE` |
| `scr_stringlist.cpp`, `SL_GetStringOfLen` | `head` | `entry` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_GetStringOfLen` | `entry` | `newEntry` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_GetStringOfLen` | `freeEntry` | `newEntry` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_GetStringOfLen` | `nextFree` | `newNext` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_FreeString` | `head` | `entry` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; newEntry != entry; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_FreeString` | `entry` | `newEntry` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; newEntry != entry; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `scr_stringlist.cpp`, `SL_FreeString` | `freeIndex` | `newNext` | `(entry->status_next & HASH_STAT_MASK) == HASH_STAT_HEAD; newEntry != entry; (newNext & HASH_STAT_MASK) == HASH_STAT_FREE` |
| `files_local.h`, `file scope` | `MAX_FILE_HANDLES` | `MAX_TOTAL_FILE_HANDLES` | `f > 0 && f < MAX_TOTAL_FILE_HANDLES` |
| `com_files.c`, `file scope` | `MAX_FILE_HANDLES` | `MAX_TOTAL_FILE_HANDLES` | `f > 0 && f < MAX_TOTAL_FILE_HANDLES; ospath` |
| `com_files.c`, `file scope` | `fs_path` | `ospath` | `f > 0 && f < MAX_TOTAL_FILE_HANDLES; ospath` |
| `com_files.c`, `FS_AddGameDirectory` | `localized` | `bLanguageDirectory` | `bLanguageDirectory &#124;&#124; (!bLanguageDirectory && !iLanguage)` |
| `com_files.c`, `FS_AddGameDirectory` | `language` | `iLanguage` | `bLanguageDirectory &#124;&#124; (!bLanguageDirectory && !iLanguage)` |
| `q_parse.cpp`, `Com_ParseSetMark` | `data_p` | `text` | `text; mark` |
| `q_parse.cpp`, `Com_ParseReturnToMark` | `data_p` | `text` | `text; mark` |
| `q_shared.c`, `ParseConfigStringToStruct` | `customHandler` | `parseSpecialFieldType` | `parseSpecialFieldType != NULL` |
| `q_shared.c`, `UnGetLeanFraction` | `f` | `fFrac` | `fFrac >= 0; fFrac <= 1.f` |
| `com_math.c`, `MatrixInverse` | `determinant` | `det` | `det` |
| `com_math.c`, `RotationToYaw` | `scale` | `r` | `r` |
| `com_math.c`, `PitchForYawOnNormal` | `normal` | `vNormal` | `vNormal[0] &#124;&#124; vNormal[1] &#124;&#124; vNormal[2]` |
| `com_memory.c`, `file scope` | `tempMark` | `mark2` | `hunk_low.temp >= hunk_low.mark2` |
| `com_memory.c`, `Hunk_AllocAlignInternal` | `align` | `alignment` | `!(((int) buf) & alignment); !(alignment & (alignment - 1))` |
| `com_memory.c`, `Hunk_AllocAlignInternal` | `block` | `buf` | `!(((int) buf) & alignment); !(alignment & (alignment - 1))` |
| `com_memory.c`, `Hunk_AllocLowAlignInternal` | `align` | `alignment` | `!(((int) buf) & 15); !(alignment & (alignment - 1)) (same aligned allocation role)` |
| `com_memory.c`, `Hunk_AllocLowAlignInternal` | `block` | `buf` | `!(((int) buf) & 15); !(alignment & (alignment - 1)) (same aligned allocation role)` |
| `scr_memorytree.cpp`, `file scope` | `scrMemTreePub.mt_buffer` | `mt_buffer` | `str >= mt_buffer && str < mt_buffer + MT_SIZE` |
| `scr_stringlist.cpp`, `file scope` | `scrMemTreePub.mt_buffer` | `mt_buffer` | `str >= mt_buffer && str < mt_buffer + MT_SIZE` |

| Old name | Current name | Original map line / full assertion |
| --- | --- | --- |
| MT_NODE_COUNT | MEMORY_NODE_COUNT | L1958/L1964/L1966: node indices are bounded by MEMORY_NODE_COUNT. |
| MT_MAX_SIZE | MEMORY_NODE_BITS | L1946/L1961: allocator size classes are bounded by MEMORY_NODE_BITS. |
| HashEntry.statusNext | HashEntry.status_next | L1973/L1976-L1979/L1983-L1984/L1989-L1990: entry->status_next is the packed status/link field. |
| MEMORY_NODE_SIZE | MT_NODE_SIZE | L1967: stringValue * MT_NODE_SIZE < MT_SIZE. Same eight-byte handle granularity as the allocator. |
| HASH_STATUS_MASK | HASH_STAT_MASK | L1974/L1976-L1977/L1979/L1983-L1984/L1987/L1990: packed entry status mask. |
| HASH_STATUS_MOVABLE | HASH_STAT_MOVABLE | L1977/L1983: masked entry status is HASH_STAT_MOVABLE. |
| HASH_STATUS_HEAD | HASH_STAT_HEAD | L1990: masked entry status is HASH_STAT_HEAD. |

### Shared headers

| Name | Header | Evidence and limits |
| --- | --- | --- |
| XSurface_s::reserved | xmodel.h | Replaces the unread unknownRenderWord at +0x28. No source consumer exists. Storage is retained without inventing renderer semantics; layout/body proofs preserve. |
| ambientColor, ambientIntensity | optiondefs.h | Worldspawn ambient key and ambient diagnostic establish roles; stale comments saying their definitions were still sunAmbient* were removed. Original source spellings remain inferred. |
| BspNode::flag53 | geometry_types.h | Sole writer marks a leaf with a coplanar portal boundary. A descriptive coordinated rename with bsp.cpp remains appropriate; original spelling is unknown. |
| SHADERFLAG_0x01/0x20/0x40 and SHADERFLAG_ALPHA_BLEND | shaders.h | Parser keywords establish sky/polygonOffset/noshadow; the ALPHA_BLEND bit actually selects GE128. Public renames must include shader/preview users. Comments now state actual semantics. |

### Engine, tracing and winding names

| TU | Descriptive function spellings |
| --- | --- |
| com_math.c | `SinCos`, `SinCos_double`, `VectorCompareEpsilon`, `_VectorLength`, `VectorDistance2D`, `VectorDistanceSquared2D`, `Perp2D`, `VectorNormalize4D`, `vectosignedyaw`, `vectopitch`, `vectosignedpitch`, `vectosignedangles`, `YawVectors`, `NormalFromPoints`, `MatrixMultiplyEquals`, `MatrixMultiply34`, `MatrixMultiply43`, `DObjSkelMatrixMultiply43`, `DObjSkel2MatrixMultiply43`, `MatrixTranspose`, `VectorRotate3x3`, `MatrixTransformVector33`, `MatrixTransformVector43`, `MatrixTransformVector44`, `MatrixTransformVector43Equals`, `RotateVector2D`, `QuatInverse`, `ConvertQuatToMat`, `QuatEigenTrace`, `AngleEigenTrace`, `QuatRatioEigenTrace`, `RotationToYaw`, `PitchToQuaternion`, `YawToQuaternion`, `RollToQuaternion`, `AngleNormalize360Accurate`, `AngleNormalize180Accurate`, `YawToAxis`, `Axis4ToAngles`, `AxisToSignedAngles`, `NormalToLatLong`, `Vec10Copy`, `Q_rint`, `VectorRotateAngles`, `VectorRotateAnglesAroundPoint`, `VectorPolar`, `VectorSnap`, `_Vector5Add`, `_Vector5Scale`, `_Vector53Copy`, `RoundToPrecision`, `PitchForYawOnNormal`, `Com_SRand`, `Com_RandFloatRange`, `Com_RandIntRange`, `gunrandom`, `MatrixInverseOrthogonal43` |
| bspfile.c | `SwapBSPBrush`, `SwapTriangleSurface`, `PrintBSPLumpSize`, `UnparseEntitiesWithOrigins` |
| com_files.c | `FS_HandleForFileStreamed`, `FS_CopyFiles` |
| trace.cpp | `Vector3d::Vector3d`, `Vector3d::operator-`, `Vector3d::operator*`, `Vector3d::operator/`, `Polygon::GetFlags`, `TraceSegment_r`, `TraceSegment` |
| vectorpool.cpp | `AllocVectors`, `Vector3d::Free`, `MakeVectorPoolPermanent`, `InitVectorPool`, `GetVectorPoolUsed`, `SetVectorPoolUsed`, `RemovePoolVectors` |
| winding.cpp | `Winding::Resize`, `Winding::AddPoint`, `Winding::Copy`, `Winding::ReverseCopy`, `Winding::BaseForPlane`, `Winding::ClipFront`, `Winding::ClipBack`, `Winding::Split`, `Winding::PlaneSide`, `Winding::IsDegenerate`, `Winding::Center`, `Winding::operator==` |
| polyfile.cpp | `Q3Map_ReadPolyFile` |
| portalfile.cpp | `Polygon::SetCellIndex`, `AddPolygonToBounds`, `Q3Map_ReadPortalFile` |

| Source names | Evidence / meaning |
| --- | --- |
| `Winding::ClipFront/ClipBack::inputCount` | Inferred snapshot of the input point count before rebuilding the winding. Natural replacement for npoints; /Od name oracle confirms it preserves the frame after removing both extra blocks. |
| `MIN_WINDING_EDGE_LENGTH`, `MIN_WINDING_CROSS_SQUARED`, `maxWorldSize` | Inferred threshold names; values 0.4f, 4.0 and 268435456.0f and their types are preserved. Cross sum is twice polygon area. |
| `VECTOR_COMPARE_EPSILON`, `MAX_BOUNDS_COORD` | Inferred names for existing math thresholds 0.001f and 262144. |
| `FS_FIRST_FILE_HANDLE`, `FS_REGULAR_FILE_HANDLES`, `FS_FIRST_STREAMED_FILE_HANDLE`, `FS_STREAMED_FILE_HANDLES` | Inferred names for handle range start/count pairs 1/50 and 51/13. No new handle rule. |
| `LOCALIZED_PAK_PREFIX_LENGTH`, `PAK_LANGUAGE_BUFFER_SIZE` | Inferred names for the 10-byte localized_ prefix and existing 64-byte rotating buffers / scan bound. |
| `MAX_MAP_PATCH_COLLISIONS`, `MAX_MAP_COLLISION_VERTS`, `MAX_MAP_OCCLUDER_INDEXES`, `COD_BSP_LIGHTMAP_CHANNELS` | Inferred names for existing BSP capacities 0x2000/0x20000/0xC000 and RGB width 3. Other BSP record-width names were already present. |
| `FreeVectorBlock`, vector-pool state and winding/member/trace/parser local spellings | Inferred names describe the observed free-list overlay, pool counters, point classifications, crossings and parse buffers. Ordinary i/j/d/d1/d2 names are not machine-generated register names. Published donor locals stay unchanged. |
| `originCopy`, `length` in UnparseEntitiesWithOrigins | Inferred local spellings. The second write-only buffer and dead initializer are retained measured forms, disclosed in RECONSTRUCTION_NOTES.md. No original-source rationale is claimed. |

### Light classes

| Names | Evidence and scope |
| --- | --- |
| `MIN_POINT_LIGHT_DISTANCE_SQUARED` | Inferred identifier; target's 256.0 clamp expressed as 16.0*16.0. Q3 light.c has a 16-unit near-source distance limit but does not prove this C++ spelling or the exact flare clamp placement. |
| `LIGHT_TRACE_IGNORE_FLAGS` (pointlight/sunlight local macro) | Inferred identifier for unchanged 0x4B. TraceSegment_r rejects polygons when their flags intersect trace.mask; it is an exclusion mask, not an asserted engine CONTENTS enum. |
| `SUN_ORIGIN_OFFSET` | Inferred identifier for unchanged 0.125 centroid offset along the light direction. |
| `delta`, `incidence`, `distanceSq`, `distance`, `cosine`, `drawRadius`, `tangentAxis` | Inferred ordinary local names for displacement, dot, length, preview and tangent-snapshot roles. No original local symbols survive. |
| `rootHalf`, `frustumNormals`, `faceRightTable`, `faceUpTable`, `faceForwardTable` | Existing inferred global names; 1/sqrt(2), six four-plane cube frusta and three basis tables establish their roles. Definitions and storage unchanged. |
| `directionalPlaneEpsilon`, `spotEdgeScale`, `adjustedSpotEdgeScale` | Existing inferred names; parallel-ray tolerance and 1/32 cone-edge coefficient are established by their expressions. |
| `numSunPlanes`, `sunPlanes`, `sunSource` | Existing inferred global spellings; clip count/array/shared source roles are proved. Bounds, linkage, order and contents unchanged. |


## Credits

- Reconstruction by Jane.
- Published by Rose.
- Thanks to Prawy (https://github.com/PrawyCoD1) for the idea for this project.
