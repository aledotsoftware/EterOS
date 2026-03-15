# EterOS - Resumen de Conocimiento del Proyecto

Basado en la exploración realizada a lo largo de la conversación, este documento recopila todo lo que el Agente actualmente "sabe" o comprende sobre el código fuente de EterOS.

## 1. Descripción General
**EterOS** es un sistema operativo custom en desarrollo, enfocado y diseñado para la arquitectura **x86_64** (con referencias a posibles ports `i386` y `aarch64` en sus scripts). Cuenta con un diseño que separa estrictamente los privilegios: un kernel que corre en Ring 0 y un espacio de usuario (userspace) construido con su propia librería estándar precompilada en Ring 3.

## 2. Componentes del Kernel (`/kernel`)

- **Punto de Arranque y Configuración Inicial:**
  - **`main.c` (`kmain`)**: El punto de entrada del kernel. Inicializa el HAL, memoria (PMM, VMM, Heap), ACPI, SMP (Multi-Procesamiento Simétrico), FS (Initrd) y finalmente levanta la primera tarea (Idle/Network/UserLoader).
  - **`arch/x86_64/hal_impl.c`**: Define la capa de abstracción de hardware, inicializa GDT, IDT, PIC, y llamadas al sistema. 
  - **Manejo de Excepciones e Interrupciones (`idt.c`, `isr.c`, `syscall_entry.asm`)**: Set de rutinas que atienden fallos (Page Faults, divisiones, etc) e interrupciones de hardware (Teclado IRQ1, Mouse IRQ12, Timer IRQ0, Serial IRQ4).

- **Gestión de Memoria (MM):**
  - Sistema compuesto por Administrador de Memoria Física (**PMM**), Administrador de Memoria Virtual/Paginación (**VMM**), y Heap Genérico. Soporta mapeo para el Framebuffer y Copy-on-Write (CoW).

- **Multitasking (Scheduler y Tareas):**
  - **`task.c` / `context_switch.asm` / `user_mode.asm`**:  Cuenta con un scheduler Round-Robin y soporte para bloqueo de tareas, "sleep" o timeouts. Soporta creación de procesos hijos (`fork`) y múltiples hilos (`clone`).
  - Utiliza registros como `MSR_FS_BASE` y `MSR_KERNEL_GS_BASE` para el contexto (TLS).
  - Posibilidad de transición hacia el Espacio de Usuario usando la instrucción `iretq` con los selectores de datos de Ring 3 (0x1B) y código (0x23).

- **Sistema de Archivos (VFS y Montajes):**
  - Infraestructura **VFS** unificada que permite montar varios sistemas de archivo:
    - **initrd**: Disco ram cargado por el bootloader (con binarios base, como `marea`, `eterland`, recursos e interfaces).
    - **devfs**, **procfs**, **shmfs**, **jfs**.
  - Cargador nativo integrado para ejecutables ELF (`fs/elf.c`).

- **Controladores Estándar (Drivers):**
  - **Dispositivos de Entrada (`input/`)**: Teclado y Mouse PS/2 (este último utiliza el puerto 0x60/0x64 e IRQ12).
  - **Video / Gráficos (`video/`)**: Con un driver de Framebuffer (`fb0`) mapeable en memoria (`sys_mmap`).
  - **Red (`net/`)**: Pila de red embebida con TCP y soporte NIC para adaptadores como el `e1000`. También incluye soporte limitado para clientes DHCP localizados.
  - **Comunicación entre procesos (IPC)**: A través de Sockets VFS y pipes virtuales (`sys_pipe2`).

- **Sistema de Syscalls:**
  - Ubicado en `arch/x86_64/syscall.c`, mapea las llamadas de la libc del espacio de usuario al kernel. Usa Tablas de Llamadas por ABI (Linux, Linux32 y nativo). Usa la instrucción optimizada `syscall`.

## 3. Espacio de Usuario (`/userspace`)

- **Librería C Estándar (`/userspace/libc/`)**: Implementación bare-metal e independiente.
  - **Syscall Wrappers** en `syscall.c` (implementa las macross que invocan a `syscall0`, `syscall1`, .., `syscall6`. Expone la API POSIX (open, read, write, close, mmap, sleep, fork, execve).
  - **Entrada de Runtime (`crt0.asm`)**: Prepara los punteros `argc`, `argv`, parsea vectores auxiliares (`auxv`) provistos por el kernel y salta a `main`.

- **Aplicaciones y Shells**:
  - Posee diversas utilidades: `eterland.c` (Demostración y Shell gráfico principal del sistema operativo), `santitravel`, `sysmon`, implementaciones `wget`.

## 4. Build System & Infraestructura
- **`build.ps1`**: Es el principal orquestador de compilación bajo entornos Windows. Detecta variables de compilador cruzado (`x86_64-elf-gcc`, `nasm`, `ld`), compila el kernel y luego compila el `/userspace` como objeto enlazado de forma adjunta.
- Crea binarios intermedios, unifica las aplicaciones en formato ELF ejecutables, y enlaza el archivo `crt0.o` previo a cada app para el salto correcto.
- Utiliza **QEMU** y **VirtualBox** como plataforma virtual probada y envía sus logs al archivo de salida serial `build/x86_64/serial.log`.

## 5. Estado y Últimas Integraciones Observadas
- Correcciones recientes para compilar `crt0.asm` usando `nasm` e incrustarlo en los objetos de librerías del espacio de usuario.
- Resolución de dependencias circulares y problemas de enlace del objeto (como `sha512` y primitivas crípticas).
- Durante revisiones recientes, el ratón PS/2 registraba su interrupción hardware (IRQ12 -> `isr_stub_mouse` en `idt.c`), usando decodificación de 3 bytes que son parseados para emitir eventos de Input.
