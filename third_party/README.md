# Small vendored file-format dependencies

The reconstruction application builds offline from these unmodified sources;
there is no CMake download or system PNG/JSON dependency. Exact file sizes,
SHA-256 hashes and upstream commits are in [manifest.json](manifest.json).

| Dependency | Pinned source | Use and license |
| --- | --- | --- |
| LodePNG | [e584a8490db1b76b81bf298c11c09d3b55d9c270](https://github.com/lvandeve/lodepng/tree/e584a8490db1b76b81bf298c11c09d3b55d9c270), version 20261006 | PNG decoding, CRC and DEFLATE; two files, 421,532 bytes; zlib license embedded in both files. |
| PicoJSON | [111c9be5188f7350c2eac9ddaedd8cca3d7bf394](https://github.com/kazuho/picojson/tree/111c9be5188f7350c2eac9ddaedd8cca3d7bf394) | JSON parsing/serialization; one 34,027-byte header; BSD-2-Clause notice embedded in the header. |

The application adds size/format/schema checks and a PicoJSON parse context that
rejects duplicate keys and excessive nesting. These are application code, not
changes to either vendored library. Full upstream copyright and license notices
are retained. Binary distributions must carry the PicoJSON license notice too.

The Python fixture renderer, PNG reader/writer and exhaustive oracle use only
the standard library, including `fractions`, `struct`, `binascii` and `zlib`.
