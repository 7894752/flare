# flare — Call of Duty Light Compiler (Byte-Identical Source)

Compiles with Visual C++ 7.0 to the exact bytes of the original 2003 `flare.exe`, with the metadata stamp described below.

A source reconstruction of `flare.exe`, the light compiler (lightmaps, light grid, model lighting, OpenGL preview) that shipped with the
2003 Call of Duty 1 map tools. The source is written so that Microsoft Visual C++ 7.0 (Visual Studio .NET 2002) reproduces the original
machine code.

This project is not affiliated with or endorsed by Activision, Infinity Ward, id Software, ZeniMax or Microsoft. "Call of Duty", "Quake" and
"Wolfenstein" are trademarks of their respective owners and are used here only to describe what the code is. No game data, no original
binary and no Microsoft software is part of this repository.

## Status

- **1,255 of 1,255** project functions compile to the same bytes and relocations as the original, checked one by one.
- The program links normally. `--identical` writes a separate copy with the recorded link timestamp, PDB GUID and PDB age (215).
  That copy is **byte-identical** to the original: 577,536 bytes, SHA-256
  `3d825c0eb656faff02dbcb6c5773253966238695774eeef6f35fa9620ef3534e`. Code, data, imports, layout and the Rich header come
  from compilation and linking; the stamp changes only the three metadata fields. The ordinary linked image is retained.
- Byte identity proves the compiled image, not the source text: where several spellings compile to the same bytes, the choice among them
  is not evidence about the original. See [docs/RECONSTRUCTION_NOTES.md](docs/RECONSTRUCTION_NOTES.md).

Details: [docs/KNOWN_DIFFERENCES.md](docs/KNOWN_DIFFERENCES.md). Third-party and library functions are accounted for separately from the project count.

## Requirements

- Windows and Python 3 (verified with Python 3.11; standard library only).
- Visual C++ 7.0, `cl.exe` 13.00.9466, with its Platform SDK and static CRT: either your own copy, or let the build download it
  (`--fetch-toolchain`, needs 7-Zip; see [build/README.md](build/README.md)). Other compilers do not give the same code.

## Build

```bat
python -I build\build.py --fetch-toolchain --strict --identical
```

or, with an installed Visual C++ 7.0:

```bat
python -I build\build.py --vc7 "C:\Program Files\Microsoft Visual Studio .NET\Vc7" --strict --identical
```

The build compiles 119 translation units from a 201-file source/header/notice set, builds `cmdlib.lib` and links the exe, then prints the SHA-256 of the result next to the
recorded release hash. It never runs the program it builds. Add `--original C:\path\to\flare.exe` to get a byte and section comparison
with your own copy if the result differs. All options, toolchain checks and link-order notes are in [build/README.md](build/README.md).

## Binaries

This repository contains no executables. The program it rebuilds is the original `flare.exe` from the Call of Duty (2003) mod tools; if you
just need the tool, use the official mod tools.

## How the match is established

Each function is compiled with the original compiler and measured flags. Its instruction bytes, relocation targets, jump tables and
C++ exception-handling contributions must agree with the original. The complete linked image is then compared byte for byte, including
headers, data, imports and padding. The per-function checks used private tooling; the standalone build needs only the source in `src/`
and the manifest in `build/sources.json`.
Where several spellings give identical bytes, the choice among them is not evidence about the original source.

## Repository layout

```
src/        the reconstruction (flare/, common/, code/, libs/, universal/, win32/)
build/      build.py, build.bat, sources.json, fetch_toolchain.py, stamp_meta.py, meta_stamp.json, README.md
docs/       known differences, notes on what is proven and what is a choice
COPYING     GPLv3 with id Software's additional terms
THIRD_PARTY_NOTICES.md
```

## License

GPL-3.0-or-later with the additional terms id Software applied to its Wolfenstein source releases: see [COPYING](COPYING). Files that
descend from the Quake III Arena source keep their original headers (GPL version 2 or, at your option, any later version), which is used
here under version 3. Third-party code (IJG jpeg 6b, zlib, minizip) is under its own terms: see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This software is based in part on the work of the Independent JPEG Group.

## Credits

- Reconstruction by Jane.
- Published by Rose.
- Thanks to Prawy (https://github.com/PrawyCoD1) for the idea for this project.

Evidence and sources:

- id Software: Quake III Arena (`q3map`, `common`, `qcommon`) and the Return to Castle Wolfenstein and Wolfenstein:
  Enemy Territory source releases, the base of the engine family.
- The GtkRadiant and q3map2 authors, whose published tools show the same family of code.
- Independent JPEG Group (libjpeg 6b), Jean-loup Gailly and Mark Adler (zlib 1.1.4), Gilles Vollant (minizip).
- Function and variable names come from strings in the binary, the published sources above and the symbol tables and link maps of other
  retail builds of the same game family, wherever the compiled image agrees; the rest are descriptive.

## Ports

This repository is the byte-identical reference build and stays that way. Ports to x64, to modern compilers or to other
platforms are welcome as forks; the hope is that someone takes this on.
