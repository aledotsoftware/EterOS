# éterOS - Sistema Operativo x86_64

éterOS es un proyecto de desarrollo de sistema operativo desde el "cero absoluto" (bare metal), diseñado para ejecutarse de forma nativa en hardware x86_64. Su filosofía se basa en la ligereza y la ejecución directa, con una arquitectura modular que permita la portabilidad futura a RISC-V y ARM.

## 🚀 Visión y Significado

El nombre éterOS proviene del concepto clásico de "éter":

- **Ligereza**: Un núcleo mínimo sin capas intermedias innecesarias.
- **Fluidez**: Optimizado para una comunicación directa con el hardware.
- **Omnipresencia**: Una base sólida que aspira a ser compatible con múltiples arquitecturas mediante una capa de abstracción limpia.

## 📂 Estructura del Proyecto

```
éterOS/
├── boot/                       # Código de arranque (Bootloader)
│   └── x86_64/
│       ├── boot.asm            # Bootloader de dos etapas (Stage 1 + Stage 2)
│       └── linker.ld           # Script de enlace para el kernel
├── kernel/                     # El núcleo del sistema (C)
│   ├── main.c                  # Punto de entrada del Kernel (kmain)
│   ├── string.c                # Funciones de cadena y memoria
│   └── drivers/                # Controladores de hardware
│       ├── video/
│       │   └── vga.c           # Driver VGA modo texto (0xB8000)
│       └── serial/
│           └── serial.c        # Driver UART 16550 (COM1)
├── include/                    # Cabeceras (.h)
│   ├── types.h                 # Tipos de datos básicos (freestanding)
│   ├── vga.h                   # API del driver VGA
│   ├── string.h                # API de cadenas y memoria
│   ├── serial.h                # API del puerto serie
│   └── io.h                    # Funciones de E/S de puertos (inline)
├── scripts/                    # Herramientas auxiliares
│   ├── setup_toolchain.sh      # Instalación del entorno de desarrollo
│   └── create_iso.sh           # Generación de imagen ISO booteable
├── build/                      # Archivos generados (no versionado)
├── .gitignore
├── Makefile                    # Automatización de la construcción
└── README.md
```

## 🛠️ Módulos Principales

### 1. Bootloader (`boot.asm`) - Dos Etapas

**Stage 1 (MBR - 512 bytes):**
- Configuración de segmentos y stack
- Habilitación de la Línea A20
- Carga de Stage 2 y el Kernel desde disco (INT 13h)
- Salto a Stage 2

**Stage 2 (4 KB):**
- Verificación de soporte Long Mode (CPUID)
- Configuración de la GDT de 32 bits → transición a Modo Protegido
- Configuración de tablas de paginación (PML4 → PDPT → PD) con identity mapping de 8 MB
- Activación de PAE y Long Mode (EFER MSR)
- Carga de GDT de 64 bits → salto a Long Mode
- Transferencia de control al kernel en 0x10000

### 2. Kernel (`main.c`)

Punto de entrada `kmain()`:
- Inicialización del terminal VGA (80x25, colores)
- Inicialización del puerto serie COM1 (38400 baud)
- Banner de inicio con arte ASCII
- Información del sistema

### 3. Driver VGA (`vga.c`)

- Escritura directa al buffer de video en 0xB8000
- 16 colores de fondo y texto
- Soporte para scroll automático
- Cursor hardware (CRT controller)
- Manejo de caracteres especiales (\\n, \\t, \\b, \\r)

### 4. Driver Serial (`serial.c`)

- UART 16550 en COM1 (0x3F8)
- 38400 baud, 8N1, FIFO habilitado
- Auto-test con loopback
- Ideal para depuración con QEMU (-serial stdio)

### 5. Utilidades (`string.c`)

- `memcpy`, `memset`, `memmove`, `memcmp`
- `strlen`, `strcpy`, `strcmp`
- `itoa`, `utoa_hex`

## 📋 Layout de Memoria

```
Dirección       | Contenido
----------------|----------------------------------
0x00000-0x003FF | IVT (Interrupt Vector Table)
0x00400-0x004FF | BIOS Data Area (BDA)
0x07C00-0x07DFF | Stage 1 - MBR (512 bytes)
0x07E00-0x09DFF | Stage 2 - Bootloader extendido
0x10000-0x1FFFF | Kernel de éterOS
0x70000-0x72FFF | Tablas de paginación (12 KB)
0x90000         | Tope del Stack
0xB8000-0xBFFFF | Buffer de video VGA (modo texto)
```

## ⚙️ Requisitos de Desarrollo

### Toolchain (Cross-Compiler)

| Herramienta | Descripción |
|---|---|
| `nasm` | Ensamblador para el bootloader |
| `x86_64-elf-gcc` | Cross-compiler C (freestanding) |
| `x86_64-elf-ld` | Enlazador |
| `x86_64-elf-objcopy` | Conversión ELF → binario plano |
| `qemu-system-x86_64` | Emulador para pruebas |
| `gdb` | Depuración remota |

### Instalación rápida

```bash
# Ubuntu/Debian
./scripts/setup_toolchain.sh

# macOS (Homebrew)
brew install nasm qemu x86_64-elf-gcc x86_64-elf-binutils
```

## 🛣️ Secuencia de Arranque

```
┌──────────────────────────────────────────────────┐
│ 1. BIOS carga el MBR (sector 0) en 0x7C00       │
│    └─▶ Stage 1 ejecuta                           │
│                                                  │
│ 2. Stage 1: Habilita A20, carga Stage 2 + Kernel │
│    └─▶ Salta a Stage 2 (0x7E00)                  │
│                                                  │
│ 3. Stage 2: Verifica Long Mode, configura GDT    │
│    └─▶ Entra en Modo Protegido (32-bit)          │
│                                                  │
│ 4. Stage 2: Configura paginación (PML4/PDPT/PD)  │
│    └─▶ Activa PAE + EFER LME                     │
│                                                  │
│ 5. Stage 2: Activa paginación, carga GDT64       │
│    └─▶ Entra en Long Mode (64-bit)               │
│                                                  │
│ 6. Long Mode: Salta a kmain() en 0x10000         │
│    └─▶ ¡Kernel de éterOS ejecutándose!            │
└──────────────────────────────────────────────────┘
```

## � Comandos de Compilación

```bash
# Construir todo el sistema
make all

# Ejecutar en QEMU
make run

# Ejecutar con depuración GDB
make debug
# En otra terminal: gdb -ex 'target remote localhost:1234'

# Limpiar archivos generados
make clean

# Ver información del build system
make info
```

## 📝 Próximos Pasos

- [ ] Finalizar la configuración de la GDT de 64 bits
- [ ] Implementar el gestor de memoria física (PMM)
- [ ] Desarrollar un sistema de interrupciones (IDT) básico
- [ ] Soporte para entrada de teclado (PS/2)
- [ ] Gestor de memoria virtual (paginación completa)
- [ ] Soporte para Framebuffer UEFI/GOP

## 📜 Licencia

Proyecto educativo de desarrollo de sistemas operativos.
