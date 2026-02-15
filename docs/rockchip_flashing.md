# Guía de Instalación para Tablets Rockchip RK3566

Esta guía explica cómo compilar y flashear **éterOS** en una tablet basada en el procesador **Rockchip RK3566**.

⚠️ **ADVERTENCIA:** El soporte ARM64 es experimental. Actualmente **NO verás nada en la pantalla** de la tablet (pantalla negra), ya que el kernel solo envía información de depuración a través del puerto **UART Serial** (pines TX/RX internos).

---

## 1. Requisitos Previos

### Software (PC)
1.  **DriverAssistant (Rockchip USB Driver):** Instala los drivers USB para que Windows reconozca el dispositivo en modo Loader/Maskrom.
2.  **RKDevTool (v2.7 o superior):** Herramienta oficial de flasheo para Windows.
3.  **Compilador Cruzado (Cross-Compiler):** Necesitas GCC para AArch64 (64-bit).
    *   **Descarga Directa:** [Arm GNU Toolchain 15.2 - AArch64 bare-metal (MSI)](https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-mingw-w64-x86_64-aarch64-none-elf.msi)
    *   *Nota:* Asegúrate de elegir **AArch64** y no solo "arm".

### Hardware
- Tablet RK3566.
- Cable USB-C de datos.
- (Opcional pero recomendado) Adaptador **USB-TTL (Serial)** para ver el log de arranque. Conectar a pines TX/RX (1500000 baudios).

---

## 2. Compilación del Kernel

Antes de flashear, debes compilar éterOS para la arquitectura ARM64.

1.  Abre una terminal PowerShell en la carpeta raíz `p:\EterOS`.
2.  Ejecuta el comando de construcción:

    ```powershell
    .\build.ps1 -Target all -Arch aarch64
    ```

3.  Si la compilación es exitosa, se generará el archivo del kernel en:
    *   `p:\EterOS\build\aarch64\kernel.bin`

---

## 3. Conexión en Modo Loader

1.  Apaga completamente la tablet.
2.  Mantén presionado el botón de **Volumen Arriba (+)** (en algunos modelos es Volumen Abajo o Reset).
3.  Sin soltar el botón, conecta el cable USB al PC.
4.  Si windows detecta un dispositivo nuevo (sonido), suelta el botón.
5.  Abre **RKDevTool**. Debería decir **"Found One LOADER Device"** en la parte inferior.
    *   Si dice "No Devices Found", revisa los drivers o intenta otro puerto/botón.
    *   Si dice "Found One MASKROM Device", es un modo diferente (más profundo), pero también sirve.

---

## 4. Flasheo (Escritura del Kernel)

El objetivo es reemplazar la partición de arranque (`boot` o `kernel`) con nuestro `kernel.bin`.

### Opción A: Usando RKDevTool (Windows)

1.  En RKDevTool, ve a la pestaña **"Download Image"**.
2.  Haz click derecho en la lista de particiones y selecciona **"Load Config"** si tienes un archivo `config.cfg` o `parameter.txt`. Si no, intenta leer la tabla de particiones del dispositivo (click derecho -> "Export Image" a veces funciona para leer, pero para escribir necesitas saber el offset).
3.  Alternativamente, usa la pestaña **"Upgrade Firmware"** si tienes una imagen completa `update.img` (éterOS aún no genera esto).
4.  **Método Recomendado (Partición Específica):**
    *   Marca la casilla de la partición **`boot`** o **`kernel`**.
    *   Haz click en la celda de la columna **Path** (ruta) de esa fila.
    *   Selecciona el archivo `p:\EterOS\build\aarch64\kernel.bin`.
    *   Haz click en **Run**.

**Nota Importante:** Si tu tablet tiene Android, el U-Boot (gestor de arranque) original podría rechazar un kernel "crudo" (`kernel.bin`) porque espera un formato específico (`Android Boot Image`). En ese caso, necesitarás empaquetar el kernel con `mkbootimg` antes de flashear, o instalar un U-Boot genérico (como el de Quartz64/Linux).

---

## 5. Verificación

1.  Reinicia la tablet.
2.  Si tienes el cable Serial Debug conectado, verás los logs de éterOS en tu terminal (1.5M baudios). O 115200 dependiendo del U-Boot.
3.  Si no tienes cable Serial, la pantalla permanecerá en negro (el driver de video LCD/HDMI para RK3566 aún no está implementado en éterOS).

---

## Solución de Problemas

- **Hundido en Bootloop:** Si la tablet se reinicia constantemente, vuelve a entrar en modo Loader y flashea la imagen de fábrica original (Android/Linux) para recuperar.
- **Error de compilación:** Asegúrate de tener `aarch64-elf-gcc` instalado y en el PATH.
