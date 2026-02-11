# éterOS — Arquitecturas Soportadas

Cada subdirectorio contiene la implementación de la HAL para una arquitectura específica.

## Estructura

```
arch/
├── x86_64/          ✅ Implementado — PCs, servidores, data centers
│   ├── idt.c         Interrupt Descriptor Table
│   ├── pic.c         8259 PIC controller
│   └── hal_impl.c    Implementación de la HAL (futuro)
│
├── aarch64/         🔜 Planificado — RPi, tablets, phones, satélites
│   ├── gic.c         Generic Interrupt Controller
│   ├── mmu.c         ARM MMU setup
│   └── hal_impl.c    Implementación de la HAL
│
├── arm-cortex-m/    🔜 Planificado — STM32, Pico, IoT industrial
│   ├── nvic.c        Nested Vectored Interrupt Controller
│   ├── systick.c     SysTick timer
│   └── hal_impl.c    Implementación de la HAL
│
├── riscv64/         🔜 Planificado — SiFive, StarFive
│   ├── plic.c        Platform-Level Interrupt Controller
│   └── hal_impl.c    Implementación de la HAL
│
├── riscv32/         🔜 Planificado — ESP32-C3
│   └── hal_impl.c
│
└── xtensa/          🔜 Planificado — ESP32, ESP32-S3
    └── hal_impl.c
```

## Cómo portar a una nueva arquitectura

1. Crear directorio `kernel/arch/<tu_arch>/`
2. Implementar todas las funciones de `include/hal.h`
3. Crear el bootloader/startup para esa plataforma
4. Agregar al build system
5. ¡Compilar y correr!

El shell, string utilities, y toda la lógica del kernel se reutilizan sin cambios.
