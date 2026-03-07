# =============================================================================
# MICRO-BASIC 2.1 - Makefile
#
# Targets:
#   make           -> linux (default)
#   make linux     -> GCC/Linux
#   make windows   -> MinGW 32-bit cross (i686-w64-mingw32-gcc)
#   make windows64 -> MinGW 64-bit cross (x86_64-w64-mingw32-gcc)
#   make dos       -> ia16-elf-gcc, normal model
#   make dos-small -> ia16-elf-gcc, SMALL_TARGET (64KB conservative)
#   make clean     -> remove all built binaries
#
# Tuning overrides (any target):
#   make linux NUM_VAR=52 CTL_DEPTH=12 SA_SIZE=32
# =============================================================================

# =============================================================================
# MICRO-BASIC 2.1 - Makefile
# =============================================================================

SRC	  = BASIC.c
CFLAGS   = -std=c99 -Wall -Wextra -O2

BIN_DIR  = bin
DIST_DIR = dist

# Optional tuning defines
ifdef NUM_VAR
  CFLAGS += -DNUM_VAR=$(NUM_VAR)
endif
ifdef CTL_DEPTH
  CFLAGS += -DCTL_DEPTH=$(CTL_DEPTH)
endif
ifdef SA_SIZE
  CFLAGS += -DSA_SIZE=$(SA_SIZE)
endif
ifdef BUFFER_SIZE
  CFLAGS += -DBUFFER_SIZE=$(BUFFER_SIZE)
endif
ifdef MAX_FILES
  CFLAGS += -DMAX_FILES=$(MAX_FILES)
endif

# -----------------------------------------------------------------------------
.PHONY: all linux windows windows64 dos dos-small clean dirs package

all: linux

dirs:
	mkdir -p $(BIN_DIR) $(DIST_DIR)

# -----------------------------------------------------------------------------
# Linux
# -----------------------------------------------------------------------------
linux: dirs $(SRC)
	mkdir -p $(BIN_DIR)/linux
	gcc $(CFLAGS) -o $(BIN_DIR)/linux/basic $(SRC) tinybeep.c -lasound 
	$(MAKE) package-linux

# -----------------------------------------------------------------------------
# Windows 32-bit
# -----------------------------------------------------------------------------
windows: dirs $(SRC)
	mkdir -p $(BIN_DIR)/windows
	i686-w64-mingw32-gcc $(CFLAGS) -o $(BIN_DIR)/windows/basic.exe $(SRC)
	$(MAKE) package-windows

# -----------------------------------------------------------------------------
# Windows 64-bit
# -----------------------------------------------------------------------------
windows64: dirs $(SRC)
	mkdir -p $(BIN_DIR)/windows64
	x86_64-w64-mingw32-gcc $(CFLAGS) -o $(BIN_DIR)/windows64/basic.exe $(SRC)
	$(MAKE) package-windows64

# -----------------------------------------------------------------------------
# DOS (normal)
# -----------------------------------------------------------------------------
dos: dirs $(SRC)
	mkdir -p $(BIN_DIR)/dos
	ia16-elf-gcc -mcmodel=small $(CFLAGS) -o $(BIN_DIR)/dos/basic.exe $(SRC) -li86
	$(MAKE) package-dos

# -----------------------------------------------------------------------------
# DOS (SMALL_TARGET)
# -----------------------------------------------------------------------------
dos-small: dirs $(SRC)
	mkdir -p $(BIN_DIR)/dos-small
	ia16-elf-gcc -mcmodel=small $(CFLAGS) -DSMALL_TARGET -o $(BIN_DIR)/dos-small/basic.exe $(SRC) -li86
	$(MAKE) package-dos-small

# -----------------------------------------------------------------------------
# Packaging rules
# -----------------------------------------------------------------------------

package-linux:
	@echo "Packaging Linux build..."
	@if [ -f README.md ]; then cp README.md $(BIN_DIR)/linux; fi
	cd $(BIN_DIR)/linux && zip -q ../../$(DIST_DIR)/linux.zip basic README.md 2>/dev/null || \
	cd $(BIN_DIR)/linux && zip -q ../../$(DIST_DIR)/linux.zip basic

package-windows:
	@echo "Packaging Windows 32-bit build..."
	@if [ -f README.md ]; then cp README.md $(BIN_DIR)/windows; fi
	cd $(BIN_DIR)/windows && zip -q ../../$(DIST_DIR)/windows.zip basic.exe README.md 2>/dev/null || \
	cd $(BIN_DIR)/windows && zip -q ../../$(DIST_DIR)/windows.zip basic.exe

package-windows64:
	@echo "Packaging Windows 64-bit build..."
	@if [ -f README.md ]; then cp README.md $(BIN_DIR)/windows64; fi
	cd $(BIN_DIR)/windows64 && zip -q ../../$(DIST_DIR)/windows64.zip basic.exe README.md 2>/dev/null || \
	cd $(BIN_DIR)/windows64 && zip -q ../../$(DIST_DIR)/windows64.zip basic.exe

package-dos:
	@echo "Packaging DOS build..."
	@if [ -f README.md ]; then cp README.md $(BIN_DIR)/dos; fi
	cd $(BIN_DIR)/dos && zip -q ../../$(DIST_DIR)/dos.zip basic.exe README.md 2>/dev/null || \
	cd $(BIN_DIR)/dos && zip -q ../../$(DIST_DIR)/dos.zip basic.exe

package-dos-small:
	@echo "Packaging DOS SMALL_TARGET build..."
	@if [ -f README.md ]; then cp README.md $(BIN_DIR)/dos-small; fi
	cd $(BIN_DIR)/dos-small && zip -q ../../$(DIST_DIR)/dos-small.zip basic.exe README.md 2>/dev/null || \
	cd $(BIN_DIR)/dos-small && zip -q ../../$(DIST_DIR)/dos-small.zip basic.exe

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------
clean:
	rm -rf $(BIN_DIR) $(DIST_DIR)

# -----------------------------------------------------------------------------
# Meta‑targets
# -----------------------------------------------------------------------------

# Build everything
all: linux windows windows64 dos dos-small

# Package everything (assumes builds already exist)
package-all: package-linux package-windows package-windows64 package-dos package-dos-small

# Clean only binaries, keep dist packages
clean-bins:
	rm -rf $(BIN_DIR)

