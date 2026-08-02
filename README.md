# libsqfs

libsqfs is a C library for programmatically creating SquashFS 4 images. It
allows callers to compose directories, regular files, symbolic links, device
nodes, FIFOs, ownership and extended attributes without first materializing
that tree on a local filesystem. Image construction can use multiple worker
threads.

The library is used by
[Abuild](https://github.com/mweidle73/abuild) as its native SquashFS writer.

## Repository status

The original repository was hosted at
`https://gitorious.org/libsqfs/libsqfs.git`. Gitorious is no longer available,
and no newer authoritative upstream has been found. This repository therefore
preserves the recovered upstream history and maintains the version used by
Abuild.

| Branch | Purpose |
| --- | --- |
| `master` | Immutable archive of the last known upstream commit, `c5c464b` |
| `abuild` | Maintained Abuild dependency, including Zstd and current-toolchain fixes |
| `abuild-gh` | GitHub-only CI, provenance checks and Pages on top of `abuild` |

The recovered upstream is independently preserved by Software Heritage in
[snapshot `48285f2d8cf74bc14b1c5d300ed5bfccbd2fbc4f`](https://archive.softwareheritage.org/swh:1:snp:48285f2d8cf74bc14b1c5d300ed5bfccbd2fbc4f/).
The scheduled provenance workflow checks that the archived snapshot still
identifies `c5c464b` as its `master` tip; it does not claim that a live upstream
still exists.

## Supported compressors

The configured build enables the compressors whose development libraries are
available:

- zlib;
- LZMA and XZ through liblzma;
- Zstandard through libzstd;
- an uncompressed test implementation.

## Build and test

On Debian Trixie, install a C toolchain, Autoconf, Automake, Libtool,
`pkg-config`, and the development packages for zlib, liblzma and libzstd. Then
run:

```sh
./autogen.sh --disable-shared
make -j8
make check
```

The maintained test suite covers image options, empty images, regular and
special files, directory layout, extended attributes, compound data items and
the public Zstd interface. The GitHub workflow additionally runs the suite
with AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer, and checks
a Zstd-compressed image with the distribution's independent `unsquashfs`
reader. It also publishes an informative HTML source-coverage report. The
report retains zero-count data for compiled sources that no test executes; no
coverage threshold is enforced.

Build the API documentation with:

```sh
make doc
```

The generated HTML starts at `doc/index.html`.

## SquashFS background

SquashFS is a compressed read-only filesystem originally created by Phillip
Lougher. A generated image can be mounted by the Linux kernel or inspected
with the `squashfs-tools` utilities. Compared with the command-line
`mksquashfs` tool, libsqfs is useful when an application needs precise control
over metadata, needs to synthesize content on demand or cannot represent the
intended image tree on the host filesystem.

The library currently does not append to existing SquashFS images or represent
sparse files.

## License

libsqfs is distributed under the GNU General Public License, version 2 or (at
your option) any later version. See the
[license text](https://github.com/mweidle73/libsqfs/blob/abuild/COPYING).

The original implementation was written by Helge Bahmann at secunet Security
Networks AG. The Git history records subsequent contributors and the
Abuild-specific maintenance history.
