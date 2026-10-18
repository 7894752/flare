# Known differences from the original `flare.exe`

Original: `flare.exe`, built 2003-10-17 with Visual C++ 7.0 (`cl.exe` 13.00.9466, `link.exe` 7.00.9466), 577,536 bytes,
SHA-256 `3d825c0eb656faff02dbcb6c5773253966238695774eeef6f35fa9620ef3534e`.

This file describes the standalone build (`build\build.py --strict --identical`) verified on 2026-10-18.
**The stamped image is IDENTICAL to the original:** 577,536 bytes, the SHA-256 above, and zero differing, added or missing bytes.
There are no remaining binary differences after the documented metadata stamp. The source-level limits below still apply.

## What the build reproduces

- All 1,255 project functions compile to the original bytes and relocations (checked function by function), including the exception-handling
  tables of the C++ objects. Third-party/library matches are accounted for separately.
- The linked image is compared section by section with the original. The object list, link order, library list and link options are in
  `build/sources.json`; the Rich header of the result lists the same tools and object counts as the original (it is produced by the compile and link
  itself, not copied).
- The CRT, `oldnames` and the Platform SDK import libraries come from your own Visual C++ 7.0 / Platform SDK install (or the files that
  `--fetch-toolchain` downloads and verifies). None of them is part of this repository.

## Metadata normalized after linking

| # | Difference | Where | Cause |
|---|---|---|---|
| 1 | Link timestamp (0x3F903D9A = 2003-10-17 19:06:02 UTC) | COFF header, debug directory | `link.exe` 7.00.9466 stamps the time of the link and has no option to fix it. |
| 2 | PDB GUID | CodeView (RSDS) record | Random per link. |
| 3 | PDB age (215) | CodeView (RSDS) record | Counts updates of the original PDB. |

`build.py --identical` copies the three recorded original values (`build/meta_stamp.json`) into a copy of the linked image
(`flare_identical.exe`). The stamp changes only those metadata fields. The ordinary linked `flare.exe` is retained separately;
it is the stamped copy that has the original full-file hash. The Rich table is produced by the toolchain, not patched.

## Status of the current tree

- The program links with a plain link (no `/FORCE`): no unresolved and no multiply defined symbols.
- **No unmatched project function remains.** DObjCalcSkel, DObjTraceline, the preview renderer and their shared helpers all pass the body checks.
- The stamped image has **zero differing, added or missing bytes**. Its `.text`, `.data`, `.rdata`, headers, imports, entry point,
  image size, and every section's raw and virtual sizes equal the original. Run the documented `--original` comparison for each tree you build.
- The Rich header lists the same tools and the same number of objects as the original in every row (C 211, C++ 62, MASM 38, thunks 155,
  implib 13, oldnames 3, linker 1).

## Byte identity is not source identity

Byte identity proves the compiled image, not the source text. Where several spellings compile to the same bytes, the choice among them is not
evidence about the original. All names that do not come from a string, a published source or a symbol table of another build of the game family are
descriptive. See [Reconstruction notes](RECONSTRUCTION_NOTES.md).

## How to check

Run from the repository root:

```text
python -I build/build.py --vc7 <Vc7> --strict --identical --original <original-exe>
python -I build/stamp_meta.py check --input <built-exe> --original <original-exe>
```

The first command compiles, links, writes `flare_identical.exe` and compares its SHA-256 with the recorded original hash.
If the hash differs, `--original` also produces a per-section comparison and a whole-file byte count. The second command compares
only Rich and debug metadata. Neither command executes an image. A reported difference does not itself change either command's exit
status; inspect the printed equality result.

## Inferred storage

Two preview globals exist only to reproduce the layout of the original `.bss`: `previewWindowInfo` (128 bytes, behind `previewGLContext`) and `previewStateSerial` (4 bytes, first variable of the state object). The shipped executable never reads or writes them; only their size and position are known, so their names, types and purpose are inferred.

The options module's eight-byte `OptionsParserState parserState` owns the
initialization status read by ExecuteOptions and set by InitOptions. Its bool
`initialized` is at +4, target 0x49A7DC. The first word, `reserved`, is
never read by the shipped executable; its type, role and spelling are inferred.
The empty constructor and layout account for the second empty initializer.
The single `options_t options` record follows at target 0x49A7E0; its name
has later-build map evidence, while its 2003 membership remains inferred.

## Inferred code

`GL_DrawBoundsOutline` in `preview_render.cpp` draws the current leaf's bounds corners under `_DEBUG`, using a `Vector3d corners[8]` array.
The release build omits that call and discards the helper's body. Its earlier array declaration still affects the compiler's function and
import order. The helper and debug call are inferred source choices; the release executable does not establish their historical existence,
names, signatures or bodies. The debug translation unit compiles, but image identity is established for the release configuration only.

## Retained original boundary limitation

The sunlight plane buffer holds 17 planes: one sky plane and at most 16 polygon-side planes. The original `SunLight::SelectFace` guard
also accepts a polygon with 17 points, which needs 18 slots. No sufficient caller invariant excluding that case has been established.
The reconstructed source preserves the original allocation and guard. Changing the guard
would be a separate behavior correction and would change the original machine code.
