; =============================================================================
; éterOS Bootloader - x86_64
; =============================================================================
; Bootloader de dos etapas que realiza la transición completa:
;   Real Mode (16-bit) → Protected Mode (32-bit) → Long Mode (64-bit)
;
; Disposición de memoria:
;   0x7C00 - 0x7DFF : Stage 1 (MBR, 512 bytes)
;   0x7E00 - 0x8FFF : Stage 2 (bootloader extendido, ~4 KB)
;   0x10000 - ...    : Kernel de éterOS
;
; Stage 1 carga Stage 2 + Kernel desde disco, luego salta a Stage 2.
; Stage 2 configura la GDT, paginación, Long Mode, y salta al kernel.
; =============================================================================

; #############################################################################
; STAGE 1 - Master Boot Record (512 bytes)
; #############################################################################
[bits 16]
[org 0x7C00]

; ---- Constantes ----
STAGE2_LOAD_ADDR    equ 0x7E00          ; Dirección donde se carga Stage 2
STAGE2_SECTORS      equ 8              ; Sectores de Stage 2 (4 KB)
KERNEL_LOAD_ADDR    equ 0x10000         ; Dirección donde se carga el kernel
KERNEL_SECTORS      equ 64             ; Sectores del kernel (32 KB)
STACK_TOP           equ 0x7C00          ; El stack crece hacia abajo

; =============================================================================
; Punto de entrada - Stage 1
; =============================================================================
stage1_start:
    ; Configurar segmentos y stack
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP
    sti

    ; Guardar unidad de arranque (proporcionada por BIOS en DL)
    mov [boot_drive], dl

    ; Mostrar mensaje de inicio
    mov si, msg_stage1
    call print_16

    ; ---- Habilitar Línea A20 ----
    call enable_a20

    ; ---- Cargar Stage 2 desde disco ----
    mov si, msg_loading_s2
    call print_16

    mov ah, 0x02                        ; BIOS: Lectura de sectores
    mov al, STAGE2_SECTORS              ; Número de sectores
    mov ch, 0                           ; Cilindro 0
    mov cl, 2                           ; Sector 2 (Stage 2 está después del MBR)
    mov dh, 0                           ; Cabeza 0
    mov dl, [boot_drive]                ; Unidad de arranque
    mov bx, STAGE2_LOAD_ADDR            ; Buffer de destino
    int 0x13
    jc s1_disk_error

    ; ---- Cargar Kernel desde disco ----
    mov si, msg_loading_kern
    call print_16

    ; Cargar en segmento 0x1000:0x0000 = dirección lineal 0x10000
    push es
    mov ax, 0x1000
    mov es, ax
    xor bx, bx                          ; ES:BX = 0x1000:0x0000 = 0x10000

    mov ah, 0x02                        ; BIOS: Lectura de sectores
    mov al, KERNEL_SECTORS              ; Número de sectores
    mov ch, 0                           ; Cilindro 0
    mov cl, 2 + STAGE2_SECTORS          ; Sector después de Stage 2
    mov dh, 0                           ; Cabeza 0
    mov dl, [boot_drive]
    int 0x13
    pop es
    jc s1_disk_error

    mov si, msg_ok
    call print_16

    ; ---- Saltar a Stage 2 ----
    jmp 0x0000:STAGE2_LOAD_ADDR

; --- Error de disco ---
s1_disk_error:
    mov si, msg_disk_err
    call print_16
    cli
    hlt

; -----------------------------------------------------------------------------
; print_16: Imprime cadena terminada en null usando BIOS INT 10h
; -----------------------------------------------------------------------------
print_16:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

; -----------------------------------------------------------------------------
; enable_a20: Habilita la Línea A20
; -----------------------------------------------------------------------------
enable_a20:
    ; Método 1: BIOS
    mov ax, 0x2401
    int 0x15
    jnc .done

    ; Método 2: Fast A20 Gate (puerto 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al

.done:
    ret

; =============================================================================
; Datos del Stage 1
; =============================================================================
msg_stage1:      db '[eterOS] Bootloader Stage 1', 13, 10, 0
msg_loading_s2:  db '  Cargando Stage 2... ', 0
msg_loading_kern:db '  Cargando Kernel... ', 0
msg_ok:          db 'OK', 13, 10, 0
msg_disk_err:    db 'ERROR DISCO!', 13, 10, 0
boot_drive:      db 0

; =============================================================================
; Firma del MBR
; =============================================================================
times 510 - ($ - $$) db 0
dw 0xAA55

; #############################################################################
; STAGE 2 - Bootloader extendido (cargado en 0x7E00)
; #############################################################################
; A partir de aquí, el código está en el sector 2+ del disco.
; Stage 2 tiene espacio suficiente para las tablas de paginación y la GDT.
; #############################################################################

stage2_start:
    mov si, s2_msg_start
    call print_16

    ; ---- Verificar soporte para Long Mode ----
    call check_long_mode

    ; ---- Información de video ----
    ; Obtener modo de video actual (para referencia)
    mov ah, 0x0F
    int 0x10
    mov [video_mode], al

    ; ---- Transición a Modo Protegido ----
    mov si, s2_msg_pmode
    call print_16

    cli                                 ; Deshabilitar interrupciones
    lgdt [gdt32_descriptor]             ; Cargar GDT de 32 bits

    mov eax, cr0
    or eax, 1                           ; Activar bit PE
    mov cr0, eax

    jmp CODE32_SEG:protected_mode_start ; Far jump a modo protegido

; -----------------------------------------------------------------------------
; check_long_mode: Verifica soporte de CPU para modo de 64 bits
; -----------------------------------------------------------------------------
check_long_mode:
    ; Verificar CPUID extendido
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; Verificar bit de Long Mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode

    mov si, s2_msg_lm_ok
    call print_16
    ret

.no_long_mode:
    mov si, s2_msg_no_lm
    call print_16
    cli
    hlt

; ---- Mensajes de Stage 2 (16-bit) ----
s2_msg_start:   db '[eterOS] Stage 2 activo', 13, 10, 0
s2_msg_lm_ok:   db '  Long Mode: Soportado', 13, 10, 0
s2_msg_no_lm:   db '  ERROR: CPU sin Long Mode!', 13, 10, 0
s2_msg_pmode:   db '  Entrando en Modo Protegido...', 13, 10, 0

video_mode:     db 0

; =============================================================================
; STAGE 2 - Modo Protegido (32-bit)
; =============================================================================
[bits 32]
protected_mode_start:
    ; Configurar segmentos de datos de 32 bits
    mov ax, DATA32_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000                    ; Stack alto para modo protegido

    ; Indicador visual en VGA: "PM" (Protected Mode) en verde
    mov word [0xB8000], 0x2F50          ; 'P' verde
    mov word [0xB8002], 0x2F4D          ; 'M' verde

    ; ---- Configurar tablas de paginación ----
    call setup_page_tables

    ; ---- Habilitar PAE ----
    mov eax, cr4
    or eax, (1 << 5)                   ; PAE bit
    mov cr4, eax

    ; ---- Cargar PML4 en CR3 ----
    mov eax, PAGE_TABLE_ADDR
    mov cr3, eax

    ; ---- Habilitar Long Mode en EFER MSR ----
    mov ecx, 0xC0000080                 ; IA32_EFER MSR
    rdmsr
    or eax, (1 << 8)                    ; LME (Long Mode Enable)
    wrmsr

    ; ---- Activar paginación ----
    mov eax, cr0
    or eax, (1 << 31)                   ; PG bit
    mov cr0, eax

    ; ---- Cargar GDT de 64 bits y saltar a Long Mode ----
    lgdt [gdt64_descriptor]
    jmp CODE64_SEG:long_mode_start

; -----------------------------------------------------------------------------
; setup_page_tables: Configura paginación con identity mapping
;   Usa páginas de 2 MB (huge pages) para mapear los primeros 8 MB
;
;   Estructura:
;     PML4[0] → PDPT (en PAGE_TABLE_ADDR + 0x1000)
;     PDPT[0] → PD   (en PAGE_TABLE_ADDR + 0x2000)
;     PD[0-3] → 4 páginas de 2 MB cada una = 8 MB
; -----------------------------------------------------------------------------
PAGE_TABLE_ADDR equ 0x70000             ; Dirección base de las tablas

setup_page_tables:
    ; Limpiar 3 páginas de 4 KB (PML4 + PDPT + PD = 12 KB)
    mov edi, PAGE_TABLE_ADDR
    xor eax, eax
    mov ecx, (4096 * 3) / 4            ; 12 KB / 4 bytes = 3072 dwords
    rep stosd

    ; PML4[0] → PDPT
    mov eax, PAGE_TABLE_ADDR + 0x1000   ; Dirección de PDPT
    or eax, 0x03                        ; Present + Writable
    mov [PAGE_TABLE_ADDR], eax

    ; PDPT[0] → PD
    mov eax, PAGE_TABLE_ADDR + 0x2000   ; Dirección de PD
    or eax, 0x03                        ; Present + Writable
    mov [PAGE_TABLE_ADDR + 0x1000], eax

    ; PD[0] → 0x000000 - 0x1FFFFF (2 MB) - Identity mapped
    mov dword [PAGE_TABLE_ADDR + 0x2000], 0x00000083  ; Present + Write + Huge

    ; PD[1] → 0x200000 - 0x3FFFFF (2 MB)
    mov dword [PAGE_TABLE_ADDR + 0x2008], 0x00200083

    ; PD[2] → 0x400000 - 0x5FFFFF (2 MB)
    mov dword [PAGE_TABLE_ADDR + 0x2010], 0x00400083

    ; PD[3] → 0x600000 - 0x7FFFFF (2 MB)
    mov dword [PAGE_TABLE_ADDR + 0x2018], 0x00600083

    ret

; =============================================================================
; GDT de 32 bits (para la transición a Modo Protegido)
; =============================================================================
gdt32_start:
    dq 0x0000000000000000               ; Descriptor nulo

gdt32_code:
    dw 0xFFFF                           ; Limit 0-15
    dw 0x0000                           ; Base 0-15
    db 0x00                             ; Base 16-23
    db 10011010b                        ; Access: Present, Ring0, Code, Readable
    db 11001111b                        ; Flags: 4K gran, 32-bit + Limit 16-19
    db 0x00                             ; Base 24-31

gdt32_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                        ; Access: Present, Ring0, Data, Writable
    db 11001111b
    db 0x00
gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

CODE32_SEG equ gdt32_code - gdt32_start
DATA32_SEG equ gdt32_data - gdt32_start

; =============================================================================
; GDT de 64 bits (para Long Mode)
; =============================================================================
gdt64_start:
    dq 0x0000000000000000               ; Descriptor nulo

gdt64_code:
    dw 0x0000                           ; Limit (ignorado en 64-bit)
    dw 0x0000                           ; Base
    db 0x00                             ; Base
    db 10011010b                        ; Access: Present, Ring0, Code, Readable
    db 00100000b                        ; Flags: Long Mode
    db 0x00                             ; Base

gdt64_data:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10010010b                        ; Access: Present, Ring0, Data, Writable
    db 00000000b
    db 0x00
gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

CODE64_SEG equ gdt64_code - gdt64_start
DATA64_SEG equ gdt64_data - gdt64_start

; =============================================================================
; STAGE 2 - Long Mode (64-bit)
; =============================================================================
[bits 64]
long_mode_start:
    ; Configurar segmentos de 64 bits
    mov ax, DATA64_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Configurar stack de 64 bits
    mov rsp, 0x90000

    ; Indicador visual en VGA: "64" en verde brillante
    mov word [0xB8004], 0x2F36          ; '6'
    mov word [0xB8006], 0x2F34          ; '4'
    mov word [0xB8008], 0x2F20          ; ' '
    mov word [0xB800A], 0x2F4F          ; 'O'
    mov word [0xB800C], 0x2F4B          ; 'K'

    ; ---- Saltar al kernel de éterOS ----
    ; El kernel fue cargado en 0x10000 por Stage 1
    mov rax, KERNEL_LOAD_ADDR
    call rax

    ; Si el kernel retorna, detener la CPU
    cli
.halt:
    hlt
    jmp .halt

; =============================================================================
; Rellenar Stage 2 hasta completar STAGE2_SECTORS sectores (4 KB)
; =============================================================================
times (STAGE2_SECTORS * 512) - ($ - stage2_start) db 0
