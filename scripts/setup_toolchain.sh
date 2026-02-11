#!/bin/bash
# =============================================================================
# éterOS - Script de configuración del entorno de desarrollo
# =============================================================================
# Instala las herramientas necesarias para compilar éterOS.
# Soporta: Ubuntu/Debian, Fedora, Arch Linux, macOS (Homebrew)
#
# Uso: ./scripts/setup_toolchain.sh
# =============================================================================

set -e

echo "============================================"
echo "  éterOS - Configuración del Toolchain"
echo "============================================"
echo ""

# Detectar sistema operativo
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ "$(uname)" == "Darwin" ]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)
echo "[INFO] Sistema detectado: $OS"
echo ""

case "$OS" in
    ubuntu|debian)
        echo "[INSTALL] Instalando dependencias para Ubuntu/Debian..."
        sudo apt update
        sudo apt install -y \
            build-essential \
            nasm \
            qemu-system-x86 \
            xorriso \
            grub-pc-bin \
            grub-common \
            mtools \
            gdb

        # Instalar cross-compiler
        echo "[INSTALL] Instalando cross-compiler x86_64-elf..."
        sudo apt install -y gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu
        ;;

    fedora)
        echo "[INSTALL] Instalando dependencias para Fedora..."
        sudo dnf install -y \
            @development-tools \
            nasm \
            qemu-system-x86 \
            xorriso \
            grub2-tools \
            mtools \
            gdb \
            gcc-x86_64-linux-gnu \
            binutils-x86_64-linux-gnu
        ;;

    arch|manjaro)
        echo "[INSTALL] Instalando dependencias para Arch Linux..."
        sudo pacman -S --needed --noconfirm \
            base-devel \
            nasm \
            qemu-system-x86 \
            xorriso \
            grub \
            mtools \
            gdb
        
        # En Arch, el cross-compiler se puede instalar desde AUR
        echo "[INFO] Para el cross-compiler, instala desde AUR:"
        echo "       yay -S x86_64-elf-gcc x86_64-elf-binutils"
        ;;

    macos)
        echo "[INSTALL] Instalando dependencias para macOS..."
        
        if ! command -v brew &> /dev/null; then
            echo "[ERROR] Homebrew no está instalado."
            echo "        Instálalo desde: https://brew.sh"
            exit 1
        fi

        brew install nasm qemu x86_64-elf-gcc x86_64-elf-binutils xorriso mtools
        ;;

    *)
        echo "[ERROR] Sistema operativo no soportado: $OS"
        echo "        Instala manualmente: nasm, x86_64-elf-gcc, qemu-system-x86_64"
        exit 1
        ;;
esac

echo ""
echo "============================================"
echo "  Verificando instalación..."
echo "============================================"
echo ""

# Verificar herramientas
check_tool() {
    if command -v "$1" &> /dev/null; then
        echo "  [OK] $1 encontrado: $(command -v $1)"
    else
        echo "  [!!] $1 NO encontrado"
    fi
}

check_tool nasm
check_tool qemu-system-x86_64
check_tool gdb

# Verificar cross-compiler (puede tener diferentes nombres)
if command -v x86_64-elf-gcc &> /dev/null; then
    echo "  [OK] x86_64-elf-gcc encontrado"
elif command -v x86_64-linux-gnu-gcc &> /dev/null; then
    echo "  [OK] x86_64-linux-gnu-gcc encontrado"
    echo "  [INFO] Ajusta CC en el Makefile a: x86_64-linux-gnu-gcc"
else
    echo "  [!!] Cross-compiler NO encontrado"
    echo "       Necesitas instalar x86_64-elf-gcc o x86_64-linux-gnu-gcc"
fi

echo ""
echo "============================================"
echo "  Configuración completada!"
echo "  Ejecuta 'make run' para compilar y probar."
echo "============================================"
