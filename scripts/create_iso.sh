#!/bin/bash
# =============================================================================
# éterOS - Script de creación de imagen ISO
# =============================================================================
# Genera una imagen ISO booteable de éterOS usando GRUB como bootloader
# secundario (para compatibilidad con hardware real).
#
# Uso: ./scripts/create_iso.sh
# =============================================================================

set -e

BUILD_DIR="build"
ISO_DIR="${BUILD_DIR}/iso"
ISO_FILE="${BUILD_DIR}/eteros.iso"

echo "[ISO] Preparando estructura de la imagen ISO..."

# Crear estructura de directorios para GRUB
mkdir -p "${ISO_DIR}/boot/grub"

# Copiar el kernel
if [ ! -f "${BUILD_DIR}/kernel.bin" ]; then
    echo "[ERROR] kernel.bin no encontrado. Ejecuta 'make kernel' primero."
    exit 1
fi

cp "${BUILD_DIR}/kernel.bin" "${ISO_DIR}/boot/kernel.bin"

# Crear configuración de GRUB
cat > "${ISO_DIR}/boot/grub/grub.cfg" << 'EOF'
# éterOS GRUB Configuration
set timeout=3
set default=0

menuentry "eterOS" {
    multiboot2 /boot/kernel.bin
    boot
}

menuentry "eterOS (Serial Debug)" {
    multiboot2 /boot/kernel.bin
    boot
}
EOF

# Generar la ISO
echo "[ISO] Generando imagen ISO..."
grub-mkrescue -o "${ISO_FILE}" "${ISO_DIR}" 2>/dev/null

echo "[ISO] Imagen ISO generada: ${ISO_FILE}"
echo "[ISO] Para ejecutar: qemu-system-x86_64 -cdrom ${ISO_FILE}"
