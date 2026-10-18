# Building flare.exe

A standalone build of the Call of Duty light compiler released on 2003-10-17. The scripts use Python 3's standard library and
Visual C++ 7.0 (`cl.exe` 13.00.9466, `link.exe` 7.00.9466). No Python packages need installing. The build never executes its output.
The original executable and Microsoft toolchain binaries are not distributed with the source.

## Status

- **Every translation unit compiles and the program links normally**, with no unresolved or multiply defined symbols.
- **All 1,255 project functions reproduce the original bytes and relocations. The stamped image is byte-identical:** 577,536 bytes, SHA-256
  `3d825c0eb656faff02dbcb6c5773253966238695774eeef6f35fa9620ef3534e`, with zero differing, added or missing bytes.
- `--identical` writes a separate `flare_identical.exe`, normalizing the recorded link timestamp, PDB GUID and PDB age 215.
  Code, data, imports, layout and the Rich table come from compilation and ordinary linking. The unstamped `flare.exe` remains available.
  The printed hash establishes identity for each build. See [Known differences](../docs/KNOWN_DIFFERENCES.md),
  [Reconstruction notes](../docs/RECONSTRUCTION_NOTES.md) for the policy and source-level limits.

## Requirements

| What | Version |
|---|---|
| Compiler | `cl.exe` **13.00.9466** (Visual C++ 7.0, Visual Studio .NET 2002) |
| Linker / librarian | `link.exe` / `lib.exe` **7.00.9466** |
| CRT | `libc.lib`, `libcmt.lib` and `oldnames.lib` from that installation; the program uses `/ML`, with `/MT` for the command library |
| Platform SDK | August 2001 headers and import libraries: `opengl32`, `glu32`, `winmm`, `kernel32`, `user32`, `gdi32` |
| Python | Python 3, verified with 3.11; standard library only |
| OS | Windows; the compiler and linker are 32-bit Windows programs |

Use your own Visual C++ 7.0 installation or `--fetch-toolchain`. The build checks selected tool/library SHA-256 values and warns on a
mismatch; `--strict` makes a mismatch an error. Other compiler builds do not reproduce the same machine code.
An original `flare.exe` is optional: `--original` reads your copy for comparisons and never executes it.

## Usage

Run these commands from the repository root. Python's isolated mode (`-I`) is supported; no Python packages need installing.

```
python -I build/build.py --fetch-toolchain --strict --identical
```

### Getting the toolchain automatically

`fetch_toolchain.py` downloads the files named in `toolchain_manifest.json` from public Internet Archive images of the VS .NET 2002
discs and the August 2001 Platform SDK. It verifies every hash and extracts only those files into `build/toolchain/`.
It needs 7-Zip and a network connection; it runs nothing it downloads. You are responsible for your own licence for those Microsoft files.

To fetch and verify the toolchain separately, then build with it:

```
python -I build/fetch_toolchain.py --7z "C:\Program Files\7-Zip\7z.exe"
python -I build/fetch_toolchain.py --verify-only
python -I build/build.py --identical --strict
```

The fetcher uses `build/toolchain/` by default. It accepts `--dest <folder>` for a separate installation and
`--work <folder>` for downloads and extraction. Previously downloaded archives with the exact manifest filenames can be placed in
the work folder (default `build/toolchain/_download/`); the fetcher verifies their size and SHA-256 before using them.
`--keep` keeps those archives for reuse. Allow at least 3 GB of free space. Use a dedicated work directory: without `--keep`, the
fetcher removes that directory when it finishes. With a custom destination, pass its `vc7` subfolder to the build.

### Using an installed toolchain

The Vc7 root contains `bin/cl.exe`, `include/`, `lib/` and `PlatformSDK/`. `--strict` checks the selected compiler, linker, librarian and CRT/import libraries;
the fetcher's `--verify-only` checks every downloaded file, including headers and support DLLs. These checks cover different file sets,
so an installed toolchain can pass `--strict` and reproduce the image while differing from the fetch manifest:

```
python -I build/build.py --vc7 "C:\Program Files\Microsoft Visual Studio .NET\Vc7" --strict --identical
python -I build/build.py --vc7 <Vc7> --strict --identical --original <original-exe>
```

### Options and outputs

| Option | Meaning |
|---|---|
| `--vc7 DIR`, `--sdk DIR` | Vc7 root (contains `bin/cl.exe`) and Platform SDK root (default `<vc7>/PlatformSDK`). Without `--vc7`, try `%VCINSTALLDIR%`, then `build/toolchain/vc7`. |
| `--fetch-toolchain` | download the VC7.0 files named in `toolchain_manifest.json` and verify every hash (needs 7-Zip; see `fetch_toolchain.py`) |
| `--7z FILE`, `--keep-downloads` | Select the 7-Zip executable or retain downloaded archives when using `--fetch-toolchain`. |
| `--strict` | Fail on a toolchain hash mismatch. |
| `--out DIR` | Output directory (default: `build/out`). |
| `--original FILE` | If the output hash differs, write and print whole-file, section and Rich-header comparisons with this file. |
| `--identical` | Also write `flare_identical.exe` with the recorded COFF/debug timestamp, RSDS GUID and age. Refuse to stamp an image with unresolved or duplicate symbols from a forced link. |
| `--jobs N` | Parallel compiler processes, clamped to 1 through 2 (default: 2). |
| `--keep-going` | Continue compiling after a failed translation unit. |
| `--link-only` | Reuse existing `<out>/obj` files. Recompile after changing sources or flags. |
| `--clean` | Delete the output directory first, including when combined with `--dry-run`. |
| `--dry-run` | Print commands without executing tools or downloading files. Output directories and a link response file are still created. |
| `--progress` | Diagnostic only: use `/FORCE:UNRESOLVED /FORCE:MULTIPLE`. An image with symbol errors gets a warning and `FORCED_LINK_NOT_A_VALID_BUILD.txt`; it is invalid. |

Outputs go to `build\out\`: `obj\*.obj`, `lib\cmdlib.lib`, `flare.exe`, `flare.map`, `flare.pdb`, and `link.log`.
With `--original`, a differing image also produces `diff_report.txt`; an identical image needs no difference report.
If the link fails, `unresolved.txt` and `multiply_defined.txt` list the symbols and the objects that reference or define them.

The scripts also provide `--help`. `build/build.bat` forwards the same options using `py -3` when it can start, otherwise `python`
from PATH. It reports a missing interpreter before starting the build and preserves the build's exit status.

## Linux / macOS (experimental, untested)

The driver selects Wine automatically when Python is running outside Windows; `--wine` supplies a different command.
This path has not been run by the maintainers. It needs Wine with 32-bit support and `winepath` for path conversion (a `Z:` drive mapping
is the fallback). Apple Silicon also needs a compatible x86 Wine environment. 7-Zip is required if you fetch the toolchain.

```sh
python3 -I build/build.py --fetch-toolchain --strict --identical
python3 -I build/build.py --vc7 /path/to/Vc7 --strict --identical
```

`--dry-run` prints the converted commands without starting the tools. A successful Windows build does not establish that the Wine path works.

## Build inputs and link order

`sources.json` is the manifest `build.py` reads: 119 translation units, their flag sets and link order, static libraries and link options.

* One object per translation unit in the manifest; **link order = address order** of the object in the original image. jpeg-6b (IJG 6b, stock
  `jconfig.vc`) and zlib are plain objects in that order, not libraries.
* Per-object compiler flag sets (all `/O2 /Ob1 /GF /Gy /Zi /ML /DWIN32 /DNDEBUG /D_CONSOLE` unless noted): `gs`/`nogs` (`/GS` or not),
  `*_ob2` (`/Ob2`), `*_od` / `*_od_ob1` (`/Od`: the unoptimised light, lightmap and preview objects), `*_ehsc*` (C++ with exceptions),
  `*_mt` (`/MT`: only the command-library object). The padding between functions in the original (0x90 optimised, 0xCC `/Od`)
  confirms each choice.
* The whole program except the command library is `/ML`: the CRT is `libc.lib`. The command library was a static library built `/MT`
  (`build\out\lib\cmdlib.lib`); its `LIBCMT` default-lib directive is cancelled by `/NODEFAULTLIB:LIBCMT` (without it libcmt pulls 24 extra CRT members).
* Link line: `/NOLOGO /MACHINE:IX86 /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /STACK:8388608,4096 /OPT:WIN98 /OPT:NOREF /OPT:NOICF
  /NODEFAULTLIB:LIBCMT /PDBALTPATH:d:\projects\mk\bin\flare.pdb`, then the objects, the import libraries `opengl32 glu32 winmm
  kernel32 user32 gdi32` and `cmdlib.lib`; `LIBC` and `OLDNAMES` come from the objects' directives. The PDB path above is the string
  embedded in the original image, not an output location: the actual PDB is written beneath `--out`. Everything else the header shows
  (image base 0x400000, section alignment 4096, relocations stripped, OS version 4.0) is the linker default; the 4096 file alignment is
  `/OPT:WIN98` (the VS .NET 2002 option "Optimize for Windows98"); `/INCREMENTAL:NO` is needed with `/DEBUG`.
* The sources call the POSIX CRT names `stricmp`, `strnicmp`, `itoa` (resolved through `oldnames.lib`), as the original did.

## `--identical`

Ordinary linking regenerates the COFF/debug-directory timestamp and the CodeView RSDS GUID and age. `--identical` applies the recorded
values in `meta_stamp.json` to a separate `flare_identical.exe`; it keeps the ordinary `flare.exe`. The original PDB age is 215.
The stamp does not change instructions, data, imports, padding or the Rich header. Those bytes come from compiling and linking the manifest.
See [Known differences](../docs/KNOWN_DIFFERENCES.md) for the three fields and the full original hash.

## Checking the result

Every build prints the result's SHA-256, the expected SHA-256, both file sizes and an explicit identical/different result.
The successful result is `RESULT: IDENTICAL to the original flare.exe`. Without `--identical`, timestamps and PDB identity normally
differ. For a differing output, `--original` reports changed bytes (including headers), added/missing tail bytes, the first differing
file offset, section sizes and Rich counts. A shifted section can produce many byte differences from one layout change.

The driver's exit status reports build success; a hash difference is printed but does not itself return an error. Likewise,
`stamp_meta.py check` reports metadata differences without using its exit status to signal inequality. Check the printed result.

If a build differs unexpectedly, use a fresh `--out` directory, `--strict` and `--identical` before comparing again. Compiler logs
are in `<out>/work/<object>/compile.log`; linker diagnostics are in `<out>/link.log`. A failed hash check usually calls for the pinned
toolchain (`--fetch-toolchain`) rather than changes to the manifest's flags or link order.

## Layout

```
build.py              the build driver (stdlib only)
build.bat             wrapper: build.bat --vc7 "...\Vc7" [options]
sources.json          compiler flags, translation units in link order, static libraries and link options
fetch_toolchain.py    optional: downloads the VC7.0 files from Internet Archive images (sha256 verified), needs 7-Zip
toolchain_manifest.json          pinned download locations, file selections, sizes and SHA-256 values
stamp_meta.py / meta_stamp.json   metadata inspection and the recorded --identical stamp policy
```

The public source tree is in `src/`. The export contains 201 source/header/notice files: 199 files in the translation-unit include closure
and the two vendor notices. Shared inline members are in the per-class headers (`vector.h`, `bounds.h`, `plane.h`, `winding.h`, `color.h`, `polygon.h`, `traceresult.h`); a few small ones are defined locally in the `.cpp` files that use them (see docs/RECONSTRUCTION_NOTES.md section 8).
Include search order is the translation unit's directory, the source root, the VC7 headers,
then the Platform SDK headers. Paths in `sources.json` are relative to the repository root; scripts work from any current directory.
