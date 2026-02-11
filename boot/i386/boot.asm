; =============================================================================
; éterOS Bootloader - i386 (32-bit Protected Mode)
; =============================================================================
; Bootloader de dos etapas para modo de 32 bits:
;   Real Mode (16-bit) → Protected Mode (32-bit)
;
; Disposición de memoria (idéntica al x86_64):
;   0x7C00 - 0x7DFF : Stage 1 (MBR, 512 bytes)
;   0x7E00 - 0x8FFF : Stage 2 (bootloader extendido, ~4 KB)
;   0x10000 - ...    : Kernel de éterOS (32-bit)
;
; Stage 1 carga Stage 2 + Kernel desde disco, luego salta a Stage 2.
; Stage 2 configura la GDT y entra en Protected Mode.
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
msg_stage1:      db '[eterOS/i386] Bootloader Stage 1', 13, 10, 0
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

stage2_start:
    mov si, s2_msg_start
    call print_16

    ; ---- Transición a Modo Protegido (sin Long Mode) ----
    mov si, s2_msg_pmode
    call print_16

    cli                                 ; Deshabilitar interrupciones
    lgdt [gdt32_descriptor]             ; Cargar GDT de 32 bits

    mov eax, cr0
    or eax, 1                           ; Activar bit PE
    mov cr0, eax

    jmp CODE32_SEG:protected_mode_start ; Far jump a modo protegido

; ---- Mensajes de Stage 2 (16-bit) ----
s2_msg_start:   db '[eterOS/i386] Stage 2 activo', 13, 10, 0
s2_msg_pmode:   db '  Entrando en Modo Protegido (32-bit)...', 13, 10, 0

; =============================================================================
; STAGE 2 - Modo Protegido (32-bit) — DESTINO FINAL
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
    mov esp, 0x90000                    ; Stack alto

    ; Indicador visual en VGA: "PM 32" en verde
    mov word [0xB8000], 0x2F50          ; 'P'
    mov word [0xB8002], 0x2F4D          ; 'M'
    mov word [0xB8004], 0x2F33          ; '3'
    mov word [0xB8006], 0x2F32          ; '2'
    mov word [0xB8008], 0x2F20          ; ' '
    mov word [0xB800A], 0x2F4F          ; 'O'
    mov word [0xB800C], 0x2F4B          ; 'K'

    ; ---- Saltar al kernel de éterOS (32-bit) ----
    mov eax, KERNEL_LOAD_ADDR
    call eax

    ; Si el kernel retorna, detener la CPU
    cli
.halt:
    hlt
    jmp .halt

; =============================================================================
; GDT de 32 bits (segmentos planos 4 GB)
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
; Rellenar Stage 2 hasta completar STAGE2_SECTORS sectores (4 KB)
; =============================================================================
times (STAGE2_SECTORS * 512) - ($ - stage2_start) db 0
