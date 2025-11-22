.PHONY: all debug release run deb clean test

all: release

debug:
	mkdir -p build
	g++ -std=c++17 -g -DDEBUG -pthread -o build/kubsh src/main.cpp src/shell.cpp src/vfs_manager.cpp

release:
	mkdir -p build
	g++ -std=c++17 -O2 -pthread -o build/kubsh src/main.cpp src/shell.cpp src/vfs_manager.cpp

run: release
	./build/kubsh

deb: release
	mkdir -p kubsh/usr/local/bin
	mkdir -p kubsh/DEBIAN
	cp build/kubsh kubsh/usr/local/bin/
	chmod 755 kubsh/usr/local/bin/kubsh
	
	echo "Package: kubsh" > kubsh/DEBIAN/control
	echo "Version: 1.0" >> kubsh/DEBIAN/control
	echo "Section: utils" >> kubsh/DEBIAN/control
	echo "Priority: optional" >> kubsh/DEBIAN/control
	echo "Architecture: amd64" >> kubsh/DEBIAN/control
	echo "Maintainer: kubsh developer" >> kubsh/DEBIAN/control
	echo "Description: Custom shell implementation" >> kubsh/DEBIAN/control
	
	chmod 644 kubsh/DEBIAN/control
	dpkg-deb --build kubsh
	mv kubsh.deb kubsh_1.0_amd64.deb

clean:
	rm -rf build kubsh kubsh.deb kubsh_1.0_amd64.deb

test: release
	echo "Testing basic functionality..."
	./build/kubsh --version || echo "No version flag"