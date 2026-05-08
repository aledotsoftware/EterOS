# kernel-stability-boot-bot

## Domain
kernel/boot, kernel/mm, kernel/arch

## Description
Boot, memoria, trampas, init y estabilidad temprana.

## Current Goal (as of 2026-05-08)
Implementar gestión de energía (ACPI S5 shutdown) en `kernel/arch/x86_64/acpi.c`. Debes parsear el `DSDT` (apuntado desde `FADT`) localizando el bloque AML `\_S5_` para extraer los verdaderos valores de `SLP_TYPa` y `SLP_TYPb`, para posteriormente usarlos en el apagado vía S5, quitando valores hardcodeados en `acpi_poweroff()`. Asegúrate también de que exista e invocarlo vía una unificada `cmd_poweroff` y `cmd_halt` en `kernel/shell/cmd_system.c`.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
