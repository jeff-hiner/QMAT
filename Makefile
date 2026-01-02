# Q-MAT WASM Build Makefile
# Usage:
#   make          - Build with -O2
#   make OPT=-O0  - Build without optimizations (for debugging)
#   make -j4      - Parallel build
#   make clean    - Remove build artifacts

# Use bash shell for consistent behavior
SHELL := /usr/bin/bash

EMSDK ?= /c/Users/prelu/git/emsdk
EMCC = $(EMSDK)/upstream/emscripten/em++.bat

# Compiler settings
CXX = $(EMCC)
# QMAT_NO_CGAL: Use simplified implementation without CGAL dependency
CXXFLAGS = -std=c++17 -Wall -Wextra -DQMAT_NO_CGAL
OPT ?= -O2

# Include paths
INCLUDES = -Isrc

# Linker flags for WASM
LDFLAGS = -sWASM=1 \
          -sEXPORTED_FUNCTIONS="['_wasm_malloc','_wasm_free','_qmat_simplify']" \
          -sEXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
          -sALLOW_MEMORY_GROWTH=1 \
          -sINITIAL_MEMORY=67108864

# Output
TARGET = qmat_buffer.js
TARGET_WASM = qmat_buffer.wasm

# Build directory
BUILDDIR = build

# Source files
SRCS = src/Wm4Math.cpp \
       src/Wm4Matrix.cpp \
       src/Wm4Vector.cpp \
       src/GeometryObjects.cpp \
       src/PrimMesh.cpp \
       src/SlabMesh.cpp \
       qmat_wasm_api.cpp

# Object files in build directory
OBJS = $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRCS))

# Default target
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
	rm -rf $(BUILDDIR) $(TARGET) $(TARGET_WASM)

# Rebuild from scratch
rebuild: clean all

# Show what would be built
info:
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
	@echo "Target: $(TARGET)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS) $(OPT)"

.PHONY: all clean rebuild info
