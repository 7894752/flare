# Third-party notices

The reconstruction as a whole is distributed under `COPYING` (GPL-3.0-or-later with id Software's additional terms).
The components below are included under their own terms.

## id Software (Quake III Arena, Return to Castle Wolfenstein, Wolfenstein: Enemy Territory)

The shared engine and tool files in `src/common`, `src/code/qcommon`, `src/code/game`, `src/universal`, `src/libs/cmdlib`,
`src/win32` and the Quake-derived routines in `src/flare` descend from code published by id Software.
Those files keep their original headers ("Copyright (C) 1999-2005 Id Software, Inc.", GNU GPL version 2 or, at your option, any later
version) and are used here under version 3. The id headers must not be removed. Some functions are close to the Return to Castle
Wolfenstein / Enemy Territory versions; for that reason the additional terms that id Software applied to those releases are part of
`COPYING`. These reconstructed and adapted files are not an original id Software or Infinity Ward source release. Source similarity alone
does not establish which version supplied an individual function in the 2003 tool.

## Independent JPEG Group, libjpeg 6b (`src/libs/jpeg6b`)

Copyright (C) 1991-1998, Thomas G. Lane. The IJG licence is in `src/libs/jpeg6b/README.ijg` ("LEGAL ISSUES") and must be kept with
redistributions. This software is based in part on the work of the Independent JPEG Group. The source set uses IJG 6b
(`jconfig.vc` as `jconfig.h`) with adjusted include paths and a memory-input adaptation in `jdatasrc.c`:
`jpeg_stdio_src` reads from a memory buffer (`unsigned char *infile, int bufsize`) instead of a `FILE`, with the matching prototype in
`src/libs/jpeg6b/jpeglib.h`. It supplies the caller's buffer directly; the published Quake III Arena and RTCW adapters refill an internal buffer.

## zlib 1.1.4 (`src/libs/zlib`)

Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler. The zlib licence (verbatim from `zlib.h`):

> This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held
> liable for any damages arising from the use of this software.
>
> Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter
> it and redistribute it freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
>    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not
>    required.
> 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
> 3. This notice may not be removed or altered from any source distribution.

The source set has two adaptations, documented in `src/libs/zlib/README.md`: `zcalloc` and `zcfree` use the engine's memory allocator;
the release's private `inflate_codes_state` definition is shared through `infcodes.h` instead of local copies or dummy compatibility
declarations. The decoder implementations are unchanged. Original per-file copyright and licence notices are retained.
The shared state is consumed by `infblock.c`, `infcodes.c`, `inffast.c` and `infutil.c`; no local duplicate or dummy state definition remains.

## minizip / unzip 0.15 (`src/code/qcommon/unzip.c`, `unzip_layout.h`)

"unzip 0.15 Copyright 1998 Gilles Vollant". The code was published by id Software inside the Quake III Arena GPL release, together
with the notice string kept in the source. This version retains both the original minizip notice (which gives the zlib conditions of use
and distribution) and the Quake-family GPL header;
the implementation is in `src/code/qcommon/unzip.c` and the archive state declarations are in `src/code/qcommon/unzip_layout.h`.
No licence from a newer minizip release is substituted for these notices.

## Not included

No Microsoft software (compiler, linker, libraries, Platform SDK headers and import libraries) and no part of the original `flare.exe`
is distributed here. `build/meta_stamp.json` stores recorded constants of the original executable (link timestamp, PDB GUID and age);
it is data, not code. The build downloads the Microsoft files it needs from public Internet Archive images only when
you ask for it with `--fetch-toolchain`; you are responsible for your own licence for them.
