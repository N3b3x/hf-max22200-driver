# Makefile for MAX22200 Driver Library
# Author: MAX22200 Driver Library
# Date: 2024

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -I include
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG

# Directories
SRC_DIR = src
INCLUDE_DIR = include
EXAMPLES_DIR = examples
BUILD_DIR = build

# Source files
LIBRARY_SOURCES = $(SRC_DIR)/MAX22200.cpp
EXAMPLE_SOURCES = $(EXAMPLES_DIR)/example_usage.cpp $(EXAMPLES_DIR)/ExampleSPI.cpp

# Object files
LIBRARY_OBJECTS = $(BUILD_DIR)/MAX22200.o
EXAMPLE_OBJECTS = $(BUILD_DIR)/example_usage.o $(BUILD_DIR)/ExampleSPI.o

# Targets
LIBRARY = $(BUILD_DIR)/libMAX22200.a
EXAMPLE = $(BUILD_DIR)/example_usage

# Default target
all: $(LIBRARY) $(EXAMPLE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Library target
$(LIBRARY): $(LIBRARY_OBJECTS) | $(BUILD_DIR)
	ar rcs $@ $^

# Example target
$(EXAMPLE): $(EXAMPLE_OBJECTS) $(LIBRARY) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Object file rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(EXAMPLES_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -c $< -o $@

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(LIBRARY) $(EXAMPLE)

# Release build
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(LIBRARY) $(EXAMPLE)

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install (copy headers and library)
install: $(LIBRARY)
	mkdir -p /usr/local/include/MAX22200
	mkdir -p /usr/local/lib
	cp $(INCLUDE_DIR)/*.h /usr/local/include/MAX22200/
	cp $(LIBRARY) /usr/local/lib/

# Uninstall
uninstall:
	rm -rf /usr/local/include/MAX22200
	rm -f /usr/local/lib/libMAX22200.a

# Run example
run: $(EXAMPLE)
	./$(EXAMPLE)

# Help
help:
	@echo "Available targets:"
	@echo "  all      - Build library and example (default)"
	@echo "  debug    - Build with debug flags"
	@echo "  release  - Build with release flags"
	@echo "  clean    - Remove build files"
	@echo "  install  - Install library and headers"
	@echo "  uninstall- Remove installed files"
	@echo "  run      - Build and run example"
	@echo "  help     - Show this help"

# Phony targets
.PHONY: all debug release clean install uninstall run help
