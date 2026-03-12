# BUILD.md — MICRO‑BASIC 2.1

## Overview

MICRO‑BASIC 2.1 builds cleanly on Linux, Windows (via MinGW cross‑compiler), and DOS (via ia16‑elf‑gcc).  
All build artifacts are placed under:

```
bin/<platform>/
```

All packaged ZIP files are placed under:

```
dist/
```

This document describes how to install the required toolchains and build all supported targets.

---

## 1. Prerequisites

### Linux (native GCC)

Ubuntu/Debian:

```
sudo apt install build-essential
```

---

### Linux (ALSA development headers)

Linux uses Alsa for beep support, install the ALSA development package:

```
sudo apt install libasound2-dev
```

This provides:

- `/usr/include/alsa/*.h`
- The ALSA PCM, mixer, and control APIs
- `-lasound` for linking

If you see errors like:

```
fatal error: alsa/asoundlib.h: No such file or directory
```

or

```
undefined reference to `snd_pcm_*'
```

then `libasound2-dev` is missing.

---

### Windows (MinGW cross‑compiler)

Install MinGW:

```
sudo apt install mingw-w64
```

This provides:

- `i686-w64-mingw32-gcc` (32‑bit)
- `x86_64-w64-mingw32-gcc` (64‑bit)

---

### DOS (ia16‑elf‑gcc)

The ia16 toolchain is provided by the **tkchia/build‑ia16** PPA.  
Install the repository and packages in this order.

#### 1. Enable the PPA

If needed:

```
sudo apt install software-properties-common
```

Add the repository:

```
sudo add-apt-repository ppa:tkchia/build-ia16
sudo apt update
```

---

#### 2. Install the compiler

```
sudo apt install ia16-elf-gcc
```

This provides:

- `ia16-elf-gcc`
- `ia16-elf-as`
- `ia16-elf-ld`
- Standard ia16 libc and headers

---

#### 3. Install the i86 runtime library (required for `-li86`)

```
sudo apt install libi86-ia16-elf
```

This installs `libi86.a`, needed for BIOS‑style routines and DOS memory‑model support.

---

#### 4. Verify installation

```
ia16-elf-gcc -print-file-name=libi86.a
```

You should see a full path such as:

```
/usr/lib/ia16-elf/lib/libi86.a
```

If the output is just `libi86.a`, the library is not installed correctly.

---

## 2. Build Targets

The Makefile supports the following targets:

| Target        | Platform / Compiler                     | Output Path                     |
|---------------|------------------------------------------|---------------------------------|
| `linux`       | GCC (native)                             | `bin/linux/basic`               |
| `windows`     | MinGW 32‑bit                             | `bin/windows/basic.exe`         |
| `windows64`   | MinGW 64‑bit                             | `bin/windows64/basic.exe`       |
| `dos`         | ia16‑elf‑gcc (normal model)              | `bin/dos/basic.exe`             |
| `dos-small`   | ia16‑elf‑gcc with `-DSMALL_TARGET`       | `bin/dos-small/basic.exe`       |
| `all`         | Build all platforms                      |                                 |
| `package-all` | Package all platforms into ZIPs          |                                 |
| `clean`       | Remove all build + dist directories      |                                 |
| `clean-bins`  | Remove only `bin/` (keep packages)       |                                 |

---

## 3. Building

### Linux

```
make linux
```

### Windows (32‑bit)

```
make windows
```

### Windows (64‑bit)

```
make windows64
```

### DOS

```
make dos
```

### DOS (SMALL_TARGET)

```
make dos-small
```

### Build everything

```
make all
```

---

## 4. Tuning Parameters

You can override internal limits at build time:

```
make linux NUM_VAR=120 CTL_DEPTH=16 SA_SIZE=64
```

Supported parameters:

- `NUM_VAR`
- `CTL_DEPTH`
- `SA_SIZE`
- `BUFFER_SIZE`
- `MAX_FILES`
- `NO_BEEP`

These become `-D` defines passed to the compiler.

---

## 5. Packaging

Each build target automatically produces a ZIP file under `dist/`.

To package everything manually:

```
make package-all
```

Each ZIP contains:

- The platform‑specific binary
- `README.md` (if present)

---

## 6. Cleaning

Remove **everything**:

```
make clean
```

Remove only binaries (keep ZIP packages):

```
make clean-bins
```

---

## 7. Directory Layout

After building all targets:

```
bin/
  linux/basic
  windows/basic.exe
  windows64/basic.exe
  dos/basic.exe
  dos-small/basic.exe

dist/
  linux.zip
  windows.zip
  windows64.zip
  dos.zip
  dos-small.zip
```

---

## 8. Troubleshooting

### Missing libi86

If you see:

```
cannot find -li86
```

Install:

```
sudo apt install libi86-ia16-elf
```

### ia16-elf-gcc not found

```
sudo apt install ia16-elf-gcc
```

### Missing ALSA headers

If you see:

```
fatal error: alsa/asoundlib.h: No such file or directory
```

Install:

```
sudo apt install libasound2-dev
```

### Missing ALSA symbols

If you see:

```
undefined reference to `snd_pcm_*'
```

Add `-lasound` to your linker flags or install the dev package above.

---

## 9. Notes

- All binaries are isolated per‑platform under `bin/`.
- All packages are reproducible and self‑contained.
- The Makefile is designed for clean cross‑platform builds with minimal dependencies.
