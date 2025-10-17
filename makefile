# Makefile для kubsh

# =============================================
# НАСТРОЙКИ КОМПИЛЯТОРА И ФЛАГОВ
# =============================================

# Компилятор C++
CXX = g++

# Флаги компиляции
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

# Флаги для отладочной сборки
DEBUG_FLAGS = -g -DDEBUG

# Флаги для релизной сборки  
RELEASE_FLAGS = -O2 -DNDEBUG

# Флаги для сборки deb-пакета
DEB_FLAGS = -O2 -DNDEBUG

# =============================================
# ПУТИ И ФАЙЛЫ
# =============================================

# Имя исполняемого файла
TARGET = kubsh

# Директория с исходниками
SRCDIR = src

# Директория для объектных файлов
OBJDIR = obj

# Находим все .cpp файлы в src/
SOURCES = $(wildcard $(SRCDIR)/*.cpp)

# Генерируем список объектных файлов
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Директория для пакета
PKGDIR = pkg

# =============================================
# ОСНОВНЫЕ ЦЕЛИ (ТАРГЕТЫ)
# =============================================

# Цель по умолчанию - собирает отладочную версию
all: debug

# Отладочная сборка
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Релизная сборка
release: CXXFLAGS += $(RELEASE_FLAGS) 
release: $(TARGET)

# =============================================
# ПРАВИЛА СБОРКИ
# =============================================

# Сборка главной цели
$(TARGET): $(OBJECTS)
	@echo "Сборка исполняемого файла $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)
	@echo "Сборка завершена!"

# Правило для компиляции .cpp файлов в .o файлы
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Запуск программы
run: debug
	@echo "Запуск kubsh..."
	./$(TARGET)

# Очистка собранных файлов
clean:
	@echo "Очистка проекта..."
	rm -rf $(OBJDIR) $(TARGET) $(PKGDIR)
	rm -f *.deb

# Установка (для пакетирования)
install: release
	@echo "Установка в систему..."
	mkdir -p $(DESTDIR)/usr/bin
	cp $(TARGET) $(DESTDIR)/usr/bin/
	chmod 755 $(DESTDIR)/usr/bin/$(TARGET)

# =============================================
# СБОРКА DEB-ПАКЕТА
# =============================================

# Создание структуры deb-пакета
deb-prepare:
	@echo "Подготовка к сборке deb-пакета..."
	mkdir -p $(PKGDIR)/DEBIAN
	mkdir -p $(PKGDIR)/usr/bin
	cp $(TARGET) $(PKGDIR)/usr/bin/
	
	# Создаем файл control для пакета
	@echo "Package: kubsh" > $(PKGDIR)/DEBIAN/control
	@echo "Version: 1.0.0" >> $(PKGDIR)/DEBIAN/control
	@echo "Section: utils" >> $(PKGDIR)/DEBIAN/control
	@echo "Priority: optional" >> $(PKGDIR)/DEBIAN/control
	@echo "Architecture: amd64" >> $(PKGDIR)/DEBIAN/control
	@echo "Maintainer: Your Name <your.email@example.com>" >> $(PKGDIR)/DEBIAN/control
	@echo "Description: Custom shell for educational purposes" >> $(PKGDIR)/DEBIAN/control
	@echo " A simple custom shell implementation with VFS user management." >> $(PKGDIR)/DEBIAN/control

# Сборка deb-пакета
deb: release deb-prepare
	@echo "Сборка deb-пакета..."
	dpkg-deb --build $(PKGDIR) kubsh_1.0.0_amd64.deb
	@echo "Пакет создан: kubsh_1.0.0_amd64.deb"

# =============================================
# ВСПОМОГАТЕЛЬНЫЕ ЦЕЛИ
# =============================================

# Показывает информацию о проекте
info:
	@echo "=== Информация о проекте ==="
	@echo "Цель: $(TARGET)"
	@echo "Исходники: $(SOURCES)"
	@echo "Объектные файлы: $(OBJECTS)"
	@echo "Компилятор: $(CXX)"
	@echo "Флаги: $(CXXFLAGS)"

# Проверка зависимостей
check-deps:
	@echo "Проверка зависимостей..."
	@which $(CXX) > /dev/null || echo "Ошибка: компилятор $(CXX) не найден"
	@which dpkg-deb > /dev/null || echo "Предупреждение: dpkg-deb не найден, сборка пакетов невозможна"
	@echo "Проверка завершена"

# =============================================
# ФОКОВЫЕ ЦЕЛИ (не создают файлов)
# =============================================
.PHONY: all debug release run clean install deb deb-prepare info check-deps