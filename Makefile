# Q-MAT Build Makefile
# Usage:
#   make          - Build WASM with -O2
#   make native   - Build native CLI executable
#   make OPT=-O0  - Build without optimizations (for debugging)
#   make -j4      - Parallel build
#   make clean    - Remove build artifacts

# Use bash shell for consistent behavior
SHELL := /usr/bin/bash

EMSDK ?= /c/Users/prelu/git/emsdk
EMCC = $(EMSDK)/upstream/emscripten/em++.bat

# Native compiler (auto-detect: prefer g++ on Windows/MSYS2)
NATIVE_CXX ?= g++

# WASM Compiler settings
CXX = $(EMCC)
# QMAT_NO_CGAL: Use simplified implementation without CGAL dependency
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -msimd128 -DQMAT_NO_CGAL
# Native: relax warnings (third-party code has GCC 13 warnings)
CXXFLAGS_NATIVE = -std=c++17 -Wall -mavx2 -DQMAT_NO_CGAL -Wno-maybe-uninitialized -Wno-unused-variable
OPT ?= -O2

# Include paths
INCLUDES = -Isrc

# Linker flags for WASM
# STANDALONE_WASM: Emit standard WASM without Emscripten JS runtime dependencies
# --no-entry: Library mode (no main function required)
# This lets wasmtime handle memory growth natively via WASI conventions
LDFLAGS = -sWASM=1 \
          -sSTANDALONE_WASM \
          --no-entry \
          -sEXPORTED_FUNCTIONS="['_wasm_malloc','_wasm_free','_qmat_simplify']" \
          -sALLOW_MEMORY_GROWTH=1 \
          -sINITIAL_MEMORY=67108864

# Output
TARGET = qmat_buffer.js
TARGET_WASM = qmat_buffer.wasm
TARGET_NATIVE = qmat_native

# Build directory
BUILDDIR = build
BUILDDIR_NATIVE = build_native

# Source files (WASM)
SRCS = src/Wm4Math.cpp \
       src/Wm4Matrix.cpp \
       src/Wm4Vector.cpp \
       src/GeometryObjects.cpp \
       src/PrimMesh.cpp \
       src/SlabMesh.cpp \
       qmat_wasm_api.cpp

# Source files (Native CLI)
SRCS_NATIVE = src/Wm4Math.cpp \
              src/Wm4Matrix.cpp \
              src/Wm4Vector.cpp \
              src/GeometryObjects.cpp \
              src/PrimMesh.cpp \
              src/SlabMesh.cpp \
              main_nocgal.cpp

# Object files in build directory
OBJS = $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRCS))
OBJS_NATIVE = $(patsubst %.cpp,$(BUILDDIR_NATIVE)/%.o,$(SRCS_NATIVE))

# Default target (WASM)
all: $(TARGET)

# Link all objects into final output
$(TARGET): $(OBJS)
	@echo "=== Linking $(TARGET) ==="
	$(CXX) $(OPT) $(LDFLAGS) $^ -o $@
	@echo "=== Build complete ==="
	@ls -la $(TARGET) $(TARGET_WASM) 2>/dev/null || dir $(TARGET) $(TARGET_WASM) 2>NUL

# Compile each source file
$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OPT) $(INCLUDES) -c $< -o $@

# Create build directory structure
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)/src

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR) $(BUILDDIR_NATIVE) $(TARGET) $(TARGET_WASM) $(TARGET_NATIVE)

# Rebuild from scratch
rebuild: clean all

# Show what would be built
info:
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
	@echo "Target: $(TARGET)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS) $(OPT)"

#==============================================================================
# Native build targets
#==============================================================================

# Build native CLI executable
native: $(TARGET_NATIVE)

# Link native executable
$(TARGET_NATIVE): $(OBJS_NATIVE)
	@echo "=== Linking $(TARGET_NATIVE) ==="
	$(NATIVE_CXX) $(CXXFLAGS_NATIVE) $(OPT) $^ -o $@
	@echo "=== Native build complete ==="
	@ls -la $(TARGET_NATIVE) 2>/dev/null || dir $(TARGET_NATIVE) 2>NUL

# Compile native objects
$(BUILDDIR_NATIVE)/%.o: %.cpp | $(BUILDDIR_NATIVE)
	@mkdir -p $(dir $@)
	$(NATIVE_CXX) $(CXXFLAGS_NATIVE) $(OPT) $(INCLUDES) -c $< -o $@

# Create native build directory
$(BUILDDIR_NATIVE):
	@mkdir -p $(BUILDDIR_NATIVE)/src

# Rebuild native
rebuild-native: clean native

.PHONY: all clean rebuild info native rebuild-native
