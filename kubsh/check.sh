#!/bin/bash
# check.sh

echo "=== Starting kubsh tests ==="

cd /workspace

echo "✓ Using pre-built kubsh from host"

# Проверяем что kubsh есть
if [ ! -f "build/kubsh" ]; then
    echo "ERROR: kubsh not found in build/"
    exit 1
fi

echo "✓ kubsh executable found"

# Копируем в PATH
cp build/kubsh /usr/local/bin/kubsh
chmod +x /usr/local/bin/kubsh

echo "✓ kubsh installed to /usr/local/bin"

# Быстрая проверка функциональности
echo "=== Quick functionality check ==="
echo -e "echo 'test'\n\\q" | timeout 3s kubsh >/dev/null 2>&1 && echo "✓ Basic execution works" || echo "✗ Basic execution failed"

# Запускаем тесты
echo "=== Running Python tests ==="
cd /opt
python3 -m pytest test_basic.py -v
python3 -m pytest test_vfs.py -v

echo "=== All tests completed ==="
