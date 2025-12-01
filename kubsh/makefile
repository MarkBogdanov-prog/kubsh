# Makefile for kubsh package with C++ compilation

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wno-unused-parameter
FUSE_CXXFLAGS = -std=c++17 -O2 -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=31

# Package settings
PACKAGE_NAME = kubsh
VERSION = 1.0.0
ARCH = amd64
BUILD_DIR = build
DEB_DIR = $(BUILD_DIR)/$(PACKAGE_NAME)-$(VERSION)
CONTROL_DIR = $(DEB_DIR)/DEBIAN
BIN_DIR = $(DEB_DIR)/usr/local/bin
MAN_DIR = $(DEB_DIR)/usr/share/man/man1

# Source files - adjust these paths to match your actual source structure
# Пример структуры, измените под ваш проект
SRC_DIR = src
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
HEADERS = $(wildcard $(SRC_DIR)/*.hpp) $(wildcard $(SRC_DIR)/*.h)

# Target binary
TARGET = $(BUILD_DIR)/kubsh

# FUSE libraries (adjust if needed)
FUSE_LIBS = -lfuse3 -lreadline

# Default target
all: build

# Build the binary
$(TARGET): $(SOURCES) $(HEADERS)
	@echo "Compiling kubsh..."
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(FUSE_CXXFLAGS) $(SOURCES) -o $(TARGET) $(FUSE_LIBS)
	@echo "Build complete"

build: $(TARGET)

# Create .deb package
deb: build
	@echo "Creating .deb package structure..."
	
	# Clean previous package if exists
	@rm -f $(PACKAGE_NAME).deb
	
	# Create directory structure
	@mkdir -p $(CONTROL_DIR)
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(MAN_DIR)
	
	# Copy binary
	cp $(TARGET) $(BIN_DIR)/
	
	# Create control file
	@echo "Package: $(PACKAGE_NAME)" > $(CONTROL_DIR)/control
	@echo "Version: $(VERSION)" >> $(CONTROL_DIR)/control
	@echo "Architecture: $(ARCH)" >> $(CONTROL_DIR)/control
	@echo "Maintainer: kubsh maintainer <maintainer@example.com>" >> $(CONTROL_DIR)/control
	@echo "Section: utils" >> $(CONTROL_DIR)/control
	@echo "Priority: optional" >> $(CONTROL_DIR)/control
	@echo "Depends: fuse3, libreadline8" >> $(CONTROL_DIR)/control
	@echo "Description: Kubernetes shell utility" >> $(CONTROL_DIR)/control
	@echo " A utility for working with Kubernetes shells" >> $(CONTROL_DIR)/control
	
	# Create postinst script for FUSE group setup
	@echo "#!/bin/bash" > $(CONTROL_DIR)/postinst
	@echo "# Post-installation script for kubsh" >> $(CONTROL_DIR)/postinst
	@echo "set -e" >> $(CONTROL_DIR)/postinst
	@echo "# Ensure user is in fuse group to use FUSE" >> $(CONTROL_DIR)/postinst
	@echo "# echo 'To use kubsh, you may need to add your user to the fuse group:'" >> $(CONTROL_DIR)/postinst
	@echo "# echo '  sudo usermod -aG fuse $$USER'" >> $(CONTROL_DIR)/postinst
	@echo "# echo 'Then log out and log back in.'" >> $(CONTROL_DIR)/postinst
	chmod +x $(CONTROL_DIR)/postinst
	
	# Create prerm script
	@echo "#!/bin/bash" > $(CONTROL_DIR)/prerm
	@echo "# Pre-removal script" >> $(CONTROL_DIR)/prerm
	@echo "set -e" >> $(CONTROL_DIR)/prerm
	chmod +x $(CONTROL_DIR)/prerm
	
	# Set permissions
	chmod 755 $(BIN_DIR)/kubsh
	
	# Build the .deb package
	dpkg-deb --build $(DEB_DIR) $(PACKAGE_NAME).deb
	
	@echo ".deb package created: $(PACKAGE_NAME).deb"
	@echo "To install: sudo dpkg -i $(PACKAGE_NAME).deb"

# Install locally (for testing)
install: deb
	sudo dpkg -i $(PACKAGE_NAME).deb || sudo apt-get install -f

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(PACKAGE_NAME).deb
	rm -f *.deb

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: FUSE_CXXFLAGS += -g -DDEBUG
debug: clean build

# Show source files
sources:
	@echo "Sources: $(SOURCES)"
	@echo "Headers: $(HEADERS)"

# PHONY targets
.PHONY: all build deb install clean debug sources