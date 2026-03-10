# 🌌 éterOS: El Sistema Operativo Universal (v0.2.0 "Genesis SMP")

éterOS es un sistema operativo de **Nueva Era**, desarrollado desde el **"cero absoluto" (Bare-Metal)**. Su arquitectura de **Kernel Híbrido** y su **HAL (Hardware Abstraction Layer)** universal le permiten ejecutarse en cualquier dispositivo: desde un microcontrolador en un sensor IoT hasta un cluster de servidores x86_64 o tablets ARM64.

> *"El éter lo llena todo, invisible e infinito. éterOS: Un sistema operativo que no conoce límites."*

---

## ✨ Filosofía y Visión: El Poder del Éter

éterOS no es solo un clon de UNIX; es una reinvención de cómo el software interactúa con el silicio. Se construye sobre tres pilares fundamentales:

1.  **Ether-Core (Arquitectura Híbrida):** Combina la estabilidad de un microkernel (aislando drivers en espacio de usuario) con el rendimiento de un núcleo monolítico para operaciones críticas.
2.  **Fluidez Nativa:** Diseñado para hardware moderno. Optimizamos para **UEFI/GOP**, **NVMe**, **Multicore (SMP)** y renderizado acelerado.
3.  **Aero-Minimalismo:** Una experiencia visual basada en vectores, transparencias (**Glassmorphism**) y una interfaz fluida denominada **Flux UI**, impulsada por el motor **Omni v2.0**.

---

## 🏛️ Arquitectura del Sistema (Radiografía Técnica)

### 1. Diagrama de Capas
```
╔════════════════════════════════════════════════════════════════════════════╗
║                        ESPACIO DE USUARIO (Ring 3)                         ║
║   ┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌────────────────────┐     ║
║   │  sh.elf  │  │ test.elf │  │ exec_test.elf│  │  Aplicaciones ELF  │     ║
║   └────┬─────┘  └────┬─────┘  └──────┬───────┘  └──────────┬─────────┘     ║
║        │             │               │                     │               ║
║   ┌────┴─────────────┴───────────────┴─────────────────────┴──────────┐    ║
║   │                    libc (Userspace Library)                       │    ║
║   └───────────────────────────┬───────────────────────────────────────┘    ║
║                        syscall (INT/SYSCALL)                               ║
╠═══════════════════════════════╪════════════════════════════════════════════╣
║                     ESPACIO DEL KERNEL (Ring 0)                            ║
║ ┌────────────────────────────────────────────────────────────────────────┐ ║
║ │  INTERFAZ DE SYSCALLS (Linux-Compatible ABI)                           │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║ ┌──────────┐  ┌─────────┐  ┌───────────┐  ┌──────────────────┐             ║
║ │SCHEDULER │  │  SHELL  │  │  NETWORK  │  │  APLICACIONES    │             ║
║ │(task.c)  │  │(shell/) │  │ (net/)    │  │  INTERNAS (apps/)│             ║
║ └────┬─────┘  └────┬────┘  └─────┬─────┘  └────────┬─────────┘             ║
║ ┌────┴──────────────┴───────┴────────┴─────────────────┴──────────┐        ║
║ │  MEMORY MGR (mm/)  │  FILESYSTEM (VFS) (fs/)  │   CRYPTO (crypto/)     │ ║
║ └─────────┬────────────────────┬─────────────────────────────────────────┘ ║
║ ┌─────────┴────────────────────┴─────────────────────────────────────────┐ ║
║ │            HARDWARE ABSTRACTION LAYER (HAL)                            │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │  DRIVERS (Video, Serial, Input, Net, Disk, Timer, PCI, RTC, ACPI)      │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │              CÓDIGO ESPECÍFICO DE ARQUITECTURA                         │ ║
║ │                   (x86_64 / AArch64 / RISC-V)                          │ ║
║ └────────────────────────────────────────────────────────────────────────┘ ║
╚════════════════════════════════════════════════════════════════════════════╝
```

### 2. Gestión de Memoria (MMU)
*   **PMM (Physical Memory Manager):** Basado en mapa de bits (4KB por página). Utiliza estrategia **Next-Fit** con rover para asignación O(1) amortizada.
*   **VMM (Virtual Memory Manager):** Paginación de 4 niveles (PML4->PDPT->PD->PT). Soporta **Copy-on-Write (CoW)** para `fork()` y **TLB Shootdown** vía IPIs en entornos SMP.
*   **Heap:** Implementación dinámica (`kmalloc`/`kfree`) con coalescencia de bloques y protección de canarios.

#### Mapa de Memoria Virtual (x86_64):
| Rango de Dirección | Descripción |
|:--- |:--- |
| `0x0000000000000000` | Espacio de Usuario (Identidad primeros 4GB) |
| `0x0000000200000000` | Código de Usuario (ELF Load Address) |
| `0x0000000300000000` | Stack de Usuario (Ring 3) |
| `0xFFFF800000000000` | Kernel High-Half (Reservado para futura implementación) |

### 3. Scheduler y Multitarea (SMP-Aware)
éterOS utiliza un planificador **Round-Robin Preemptivo** diseñado para escalar:
*   **Preempción:** IRQ0 (PIT a 100Hz) activa el cambio de contexto.
*   **Context Switch:** Implementado en ASM para máxima velocidad, guarda/restaura registros de propósito general y FPU.
*   **SMP Support:** Despierta núcleos AP vía ACPI/MADT usando un trampolín de 16 bits. Cada CPU tiene su propio stack de kernel y bloque de datos `GS_BASE`.

### 4. VFS (Virtual File System)
Estructura de archivos unificada con múltiples backends:
*   `/` (Root): **Initrd** (Read-Only, cargado en boot).
*   `/dev/`: **DevFS** (null, zero, tty, random, input).
*   `/proc/`: **ProcFS** (uptime, version, meminfo).
*   `/data/`: **JFS** (Journaling File System) o **FAT32** (Storage).

---

## 🎨 Eterland y Flux UI

**Eterland** es el sucesor del servidor gráfico monolítico, basado en **Buffers Compartidos (SHM)** y **Cero Copia**.

*   **Motor Omni v2.0:** Soporta Alpha Blending, sombras suaves y **Dirty Region Tracking** (ahorro del 80% de ancho de banda).
*   **Protocolo:** Comunicación asíncrona vía Unix Domain Sockets (`AF_UNIX`).
*   **Aislamiento:** Cada ventana reside en su propio espacio de memoria, protegida por el kernel.

---

## 📦 EterStore y Compatibilidad Total (ACS)

éterOS busca la utilidad inmediata mediante sus **Aether Compatibility Subsystems (ACS)**:

1.  **Aether-Linux:** Traducción de ~70 syscalls Linux (RAX para el número, RDI/RSI/RDX para argumentos). Ejecuta binarios ELF64 sin modificaciones.
2.  **Aether-Droid:** Soporte para **Binder IPC** y **ART Shim** para ejecutar aplicaciones de Android (en desarrollo).
3.  **Aether-Win32:** Cargador de binarios PE (`.exe`) y puente para APIs de Windows (Kernel32.dll, User32.dll).
4.  **EterStore:** Gestor de paquetes que instala `.deb` (vía mapeo de VFS) y el formato nativo **.epk** (imágenes montables con Zstd).

---

## 🚀 Hoja de Ruta (Roadmap)

### Fase 1: Cimientos ✅
*   [x] Long Mode x86_64, PMM/VMM/Heap.
*   [x] SMP (Symmetric Multiprocessing) y ACPI.
*   [x] GDT, IDT, PIT, PIC, RTC.

### Fase 2: Hardware y E/S ✅
*   [x] Drivers: VGA, Framebuffer, PS/2 Keyboard/Mouse, PCI, E1000.
*   [x] Networking: lwIP 2.2.0 (TCP/IP, DHCP, DNS, HTTP client).
*   [x] VFS: Initrd, DevFS, ProcFS, FAT32.

### Fase 3: Userland y POSIX ✅
*   [x] Syscalls `SYSCALL`/`SYSRET`.
*   [x] ELF Loader y Mini-LibC.
*   [x] Futexes (Fast Userspace Mutexes) y Semáforos.

### Fase 4: Ecosistema y Estética (Actual) 🚧
*   [ ] Eterland Zero-Copy Compositor.
*   [ ] Flux UI (Desktop Environment).
*   [ ] ACS: Aether-Linux robusto (Copy-on-Write perfeccionado).

---

## 🛠️ Tabla de Syscalls (Estado)

| ID | Syscall | Estado | Notas |
|:--- |:--- |:--- |:--- |
| 0 | `read` | ✅ | Soporta VFS, Sockets, Pipes |
| 1 | `write` | ✅ | Salida a TTY, Serial y Archivos |
| 2 | `open` | ✅ | Creación y apertura de nodos |
| 57 | `fork` | ✅ | Clonación de proceso con CoW |
| 59 | `execve` | ✅ | Cargador ELF64 completo |
| 60 | `exit` | ✅ | Limpieza de recursos y códigos de retorno |
| 202 | `futex` | ✅ | Sincronización rápida Ring 3 |

---

## 📱 Soporte de Hardware Específico

### Rockchip RK3566 (ARM64)
éterOS es capaz de arrancar en tablets basadas en RK3566.
*   **Estado:** Experimental (Serial Debug únicamente).
*   **Flasheo:** 
    1. Compilar: `.\build.ps1 -Target all -Arch aarch64`.
    2. Conectar en modo **Loader** (Volumen + y conectar USB).
    3. Usar **RKDevTool** para escribir `kernel.bin` en la partición `boot`.
    4. Baudrate: 1.5M baudios para log de arranque.

---

## 🔧 Compilación y Ejecución

### Requisitos (Ubuntu/Debian)
```bash
sudo apt install build-essential nasm qemu-system-x86 xorriso mtools
```

### Comandos de Makefile
*   `make all`: Compila boot, kernel e imagen.
*   `make run`: Inicia en QEMU con networking habilitado.
*   `make iso`: Genera `eteros.iso` para hardware real.

### PowerShell (Windows Nativo)
*   `.\build.ps1 -Target run`: Compila y arranca en QEMU.
*   `.\build.ps1 -Target vbox`: Genera imagen VDI para VirtualBox.

---

## 📂 Estructura del Proyecto
*   `/boot`: El despertar del metal (x86_64 / ARM64).
*   `/kernel`: Ether-Core (mm, fs, net, arch, drivers, shell).
*   `/include`: API del sistema y headers de la HAL.
*   `/userspace`: Mini-LibC y aplicaciones Ring 3.
*   `/tests`: Suite masiva de pruebas unitarias y benchmarks.

---

## 📜 Licencia

© 2026 **Tudex Networks**. Todos los derechos reservados.
*"Hacia el infinito, a través del éter."*
