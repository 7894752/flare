# zlib 1.1.4

By Jean-loup Gailly and Mark Adler. Upstream source:
[zlib v1.1.4](https://github.com/madler/zlib/tree/v1.1.4).
Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler. Distributed under the zlib licence; the complete terms are in
[zlib.h](zlib.h). Keep the original copyright and licence notices with the source.

This copy contains two integration changes:

- In `zutil.c`, `zcalloc` calls `Z_MallocInternal(items * size)`, which clears the memory and applies the engine's allocation-failure
  policy; `zcfree` calls `Z_Free`.
- The private `inflate_codes_state` definition from the release's `infcodes.c` is shared through `infcodes.h`.
  Its implementation files use that one definition instead of local copies or dummy compatibility declarations.
  The decoder implementations are unchanged.

This note marks the altered source versions as required by the zlib licence.
