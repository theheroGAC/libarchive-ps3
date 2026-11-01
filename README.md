# libarchive for PlayStation 3 (PSL1GHT)

[![Platform](https://img.shields.io/badge/Platform-PlayStation%203-003791.svg?style=flat-square&logo=playstation)](https://github.com)
[![Architecture](https://img.shields.io/badge/Arch-Cell%20PPU%20(64--bit%20Big--Endian)-informational.svg?style=flat-square)](https://github.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-PSL1GHT%20v2-orange.svg?style=flat-square)](https://github.com)
[![Hardware Verified](https://img.shields.io/badge/Hardware-100%25%20Verified%20on%20Real%20PS3-brightgreen.svg?style=flat-square)](https://github.com)
[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg?style=flat-square)](LICENSE)

A complete, production-ready port of **libarchive 3.8.11** and its underlying compression backends for the Sony **PlayStation 3**, targeting the open-source **PSL1GHT v2** toolchain (`powerpc64-ps3-elf`). (port by theheroGAC)

This port includes full static libraries, headers, pkg-config configuration, and an interactive **Real-Hardware Verification Self-Test Homebrew** featuring on-screen graphics, animated progress bar, in-memory stream decompression, and physical disk extraction tests.

---

## Supported Formats & Compression Filters

All formats below have been compiled for the Cell Broadband Engine PPU (64-bit Big-Endian) and verified on real PS3 hardware (CFW & PS3HEN):

| Format / Filter | Algorithm / Container | Compression Engine | Hardware Status |
| :--- | :--- | :--- | :---: |
| **TAR** | POSIX ustar / pax | libarchive native | **PASSED** |
| **TAR.GZ** | Gzip / Deflate (RFC 1952) | `libz.a` (zlib 1.3.1) | **PASSED** |
| **TAR.BZ2** | Bzip2 / Burrows-Wheeler | `libbz2.a` (bzip2 1.0.8) | **PASSED** |
| **TAR.XZ** | XZ / LZMA2 multi-stream | `liblzma.a` (xz 5.8.4) | **PASSED** |
| **TAR.LZMA** | Legacy raw LZMA stream | `liblzma.a` (xz 5.8.4) | **PASSED** |
| **TAR.ZST** | Zstandard / Facebook zstd | `libzstd.a` (zstd 1.5.7) | **PASSED** |
| **TAR.LZ4** | LZ4 high-throughput block | `liblz4.a` (lz4 1.10.0) | **PASSED** |
| **ZIP** | PKWare ZIP / Deflate | `libarchive` + `libz` | **PASSED** |
| **CPIO.GZ** | POSIX odc cpio + gzip | `libarchive` + `libz` | **PASSED** |
| **7Z** | 7-Zip container / LZMA | `libarchive` + `liblzma` | **PASSED** |

---

## Repository Structure

```
libarchive-ps3/
├── portlibs/                   # Ready-to-use prebuilt PPU libraries & headers
│   ├── include/
│   │   ├── archive.h           # Core libarchive public API
│   │   ├── archive_entry.h     # Archive entry metadata API
│   │   ├── zlib.h, zconf.h     # Deflate compression
│   │   ├── bzlib.h             # Bzip2 compression
│   │   ├── lzma.h, lzma/       # LZMA / XZ compression
│   │   ├── zstd.h, zdict.h     # Zstandard compression
│   │   └── lz4.h, lz4frame.h   # LZ4 compression
│   └── lib/
│       ├── libarchive.a        # Main libarchive static library
│       ├── libz.a              # Zlib compression engine
│       ├── libbz2.a            # Bzip2 compression engine
│       ├── liblzma.a           # XZ / LZMA compression engine
│       ├── libzstd.a           # Zstandard compression engine
│       ├── liblz4.a            # LZ4 compression engine
│       └── pkgconfig/
│           └── libarchive.pc   # pkg-config definition
├── examples/
│   └── ps3_selftest/           # Complete verification homebrew application
│       ├── main.c              # SDL2 UI, progress bar & disk extraction test
│       ├── font8x8.h           # Bitmap font
│       ├── SDL_PSL1GHTrender.o # SDL2 hardware renderer
│       ├── Makefile            # PS3 build rules
│       └── pkgfiles/           # ICON0, PIC1 and sample test archives
├── releases/                   # Pre-compiled ready-to-run packages
│   ├── libarchive_ps3_test.pkg # Retail HMAC 0x80 PKG for CFW & PS3HEN
│   └── libarchive_ps3_test.self# Standalone ELF/SELF for multiMAN / IRISMAN
├── scripts/
│   ├── install.sh              # 1-command installer into PSL1GHT
│   ├── build.sh                # Upstream cross-compiler script
│   ├── fetch.sh                # Tarball source fetcher
│   ├── compat/                 # PSL1GHT libc compatibility shims
│   └── patches/                # Upstream source patches
├── LICENSE                     # BSD 2-Clause License
└── README.md                   # This documentation
```

---

## Quick Installation into PSL1GHT

To install `libarchive` and all compression headers/libraries directly into your PSL1GHT toolchain:

### Automatic Installation
Run the provided installer script inside WSL or Linux:

```bash
cd libarchive-ps3
bash scripts/install.sh
```

By default, this installs into `/usr/local/ps3dev/portlibs/ppu`. You can also specify a custom path:

```bash
bash scripts/install.sh /opt/ps3dev/portlibs/ppu
```

### Manual Installation
Copy the files into your PSL1GHT portlibs directory:

```bash
cp -r portlibs/include/* /usr/local/ps3dev/portlibs/ppu/include/
cp portlibs/lib/*.a /usr/local/ps3dev/portlibs/ppu/lib/
cp portlibs/lib/pkgconfig/* /usr/local/ps3dev/portlibs/ppu/lib/pkgconfig/
```

---

## How to Use in Your PS3 Project

### Makefile Configuration

Add the include and library flags to your homebrew `Makefile`:

```makefile
# Include directories
CFLAGS += -I$(PS3DEV)/portlibs/ppu/include

# Linker flags
LDFLAGS += -L$(PS3DEV)/portlibs/ppu/lib

# Libraries (order matters: libarchive first, then compression engines)
LIBS += -larchive -lz -lbz2 -llzma -lzstd -llz4
```

### Basic C Code Example

Extracting an archive file directly on the PS3:

```c
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>

int extract_archive(const char *filename, const char *out_dir)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, filename, 10240) != ARCHIVE_OK) {
        printf("Failed to open: %s\n", archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, archive_entry_pathname(entry));

        printf("Extracting: %s\n", out_path);
        FILE *f = fopen(out_path, "wb");
        if (f) {
            const void *buff;
            size_t size;
            la_int64_t offset;
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                fwrite(buff, 1, size, f);
            }
            fclose(f);
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    return 0;
}
```

---

## Real-Hardware Verification Self-Test

The repository includes a dedicated PS3 test homebrew (`examples/ps3_selftest`):

### Features
1. **Interactive SDL2 Display**: 1280x720 32-bit framebuffer with real-time test status badges.
2. **Real-time Progress Bar**: Displays extraction and verification percentage smoothly across tests.
3. **Dual-Stage Verification**:
   - **Stage 1 (RAM)**: Decompresses the binary stream in memory and checks byte-by-byte payload integrity.
   - **Stage 2 (Physical Disk)**: Physically writes the extracted files to disk (`dev_usb000` or `/dev_hdd0/tmp/`), then re-reads the files directly from the PS3 storage medium to verify filesystem integrity.
4. **Extraction Report**: Generates `EXTRACTION_INFO.txt` inside the extraction directory containing details and paths for all unpacked files.
5. **Controller Navigation**: Gamepad loop allowing inspection of results, with automatic XMB return on `Cross (X)`, `Circle (O)`, or `START`.

### Installing the Test PKG
1. Copy `releases/libarchive_ps3_test.pkg` to the root of a FAT32 USB drive.
2. Insert the USB drive into your PS3.
3. On the PS3 XMB, go to `Game` -> `Package Manager` -> `Standard` -> install `libarchive_ps3_test.pkg`.
4. Launch the app from the XMB.

---

## PSL1GHT Compatibility Details

Standard Unix systems provide POSIX APIs that PSL1GHT's lightweight newlib implementation either omits or declares differently. This port includes a robust compatibility layer (`scripts/compat/ps3_compat.c`):
- `strtoimax` and `strtoumax` symbol definitions for 64-bit integer parsing.
- Stubs for POSIX file permissions (`fchmod`, `lchmod`, `fchown`).
- User/group lookup fallback (`getuid`, `getgid`, `getpwuid`, `getgrgid`).
- Thread and mutex compatibility stubs for single-threaded PPU applications.
- Missing symbol `bz_internal_error` integration for `libbz2.a`.

---

## Building from Source

To rebuild everything from scratch using WSL or Linux with PSL1GHT installed:

```bash
# 1. Download upstream source archives
bash scripts/fetch.sh

# 2. Compile all libraries and patch libarchive
bash scripts/build.sh

# 3. Compile the test PKG
cd examples/ps3_selftest
make clean && make pkg
package_finalize libarchive_ps3_test.pkg
```

---

## License

- **libarchive**: Distributed under the [BSD 2-Clause License](LICENSE).
- **zlib**: zlib License (Jean-loup Gailly and Mark Adler).
- **bzip2**: BSD-style License (Julian Seward).
- **xz / liblzma**: Public Domain / LGPL.
- **zstd**: BSD 3-Clause License (Meta Platforms, Inc.).
- **lz4**: BSD 2-Clause License (Yann Collet).
- **PSL1GHT port & self-test**: Public Domain / BSD 2-Clause.
