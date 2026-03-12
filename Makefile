# =============================================================================
# ENHANCED MICRO-BASIC 2.3 - Makefile
#
# Targets:
#   make           -> linux (default)
#   make linux     -> GCC/Linux
#   make windows   -> MinGW 32-bit cross (i686-w64-mingw32-gcc)
#   make windows64 -> MinGW 64-bit cross (x86_64-w64-mingw32-gcc)
#   make dos       -> ia16-elf-gcc, PC DOS target (256-640K)
#   make build-all -> all of the above
#   make clean     -> remove all built binaries and packages
#
# Tuning overrides (any target):
#   make linux NUM_VAR=52 CTL_DEPTH=12 SA_SIZE=80
#
# Note: SMALL_TARGET is reserved for the 3.0 embedded port (ATmega/bare metal).
#   It is NOT used for the DOS target -- a standard PC DOS environment has
#   256-640K of conventional memory, which is ample for the normal build.
#   When 3.0 targets bare metal, SMALL_TARGET will require a full I/O HAL
#   replacement (no stdio, no malloc, fixed workspace).  See PORTING.md.
# =============================================================================

SRC      = BASIC.c
CFLAGS   = -std=c99 -Wall -Wextra -O2

BIN_DIR  = bin
DIST_DIR = dist
DOCS_DIR = documents

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
.PHONY: all linux windows windows64 dos clean dirs \
        package-linux package-windows package-windows64 package-dos \
        build-all package-all clean-bins

# Default: linux only
all: linux

dirs:
	mkdir -p $(BIN_DIR) $(DIST_DIR)

# -----------------------------------------------------------------------------
# Shared documentation staging helper.
# Usage: $(call stage-docs,$(BIN_DIR)/linux)
# Copies README.md, the BNF grammar, and the documents/ folder into target dir.
# -----------------------------------------------------------------------------
define stage-docs
	@if [ -f README.md ]; then cp README.md $(1); fi
	@if [ -f MICRO-BASIC.bnf ]; then cp MICRO-BASIC.bnf $(1); fi
	@if [ -d $(DOCS_DIR) ]; then cp -r $(DOCS_DIR) $(1)/$(DOCS_DIR); fi
endef

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
# DOS - PC target (256-640K conventional memory)
# -----------------------------------------------------------------------------
dos: dirs $(SRC)
	mkdir -p $(BIN_DIR)/dos
	ia16-elf-gcc -mcmodel=small $(CFLAGS) -o $(BIN_DIR)/dos/basic.exe $(SRC) -li86
	$(MAKE) package-dos

# -----------------------------------------------------------------------------
# Packaging rules
# Each target stages README.md + BNF + documents/ then zips everything.
# -----------------------------------------------------------------------------

package-linux:
	@echo "Packaging Linux build..."
	$(call stage-docs,$(BIN_DIR)/linux)
	cd $(BIN_DIR)/linux && zip -qr ../../$(DIST_DIR)/linux.zip .

package-windows:
	@echo "Packaging Windows 32-bit build..."
	$(call stage-docs,$(BIN_DIR)/windows)
	cd $(BIN_DIR)/windows && zip -qr ../../$(DIST_DIR)/windows.zip .

package-windows64:
	@echo "Packaging Windows 64-bit build..."
	$(call stage-docs,$(BIN_DIR)/windows64)
	cd $(BIN_DIR)/windows64 && zip -qr ../../$(DIST_DIR)/windows64.zip .

package-dos:
	@echo "Packaging DOS build..."
	$(call stage-docs,$(BIN_DIR)/dos)
	cd $(BIN_DIR)/dos && zip -qr ../../$(DIST_DIR)/dos.zip .

# -----------------------------------------------------------------------------
# Meta-targets
# -----------------------------------------------------------------------------

# Build all platforms
build-all: linux windows windows64 dos

# Package all platforms (assumes builds already done)
package-all: package-linux package-windows package-windows64 package-dos

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------

# Remove everything - binaries and packages
clean:
	rm -rf $(BIN_DIR) $(DIST_DIR)

# Remove only binaries, keep dist packages
clean-bins:
	rm -rf $(BIN_DIR)