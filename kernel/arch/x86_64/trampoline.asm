; =============================================================================
; éterOS SMP Trampoline (APs Startup Code)
; =============================================================================
; Este código corre en Real Mode (16-bit) cuando un AP despierta via SIPI.
; Debe ser copiado a una dirección alineada a página < 1MB (ej 0x8000).
; =============================================================================

[bits 16]

SECTION .text
global trampoline_start
global trampoline_end

trampoline_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; La dirección de carga real será pasada por el kernel
    ; Pero para el salto largo modo protegido necesitamos saberla.
    ; Usamos una técnica de relocalización simple.
    
    ; 1. Cargar GDT de 32 bits temporal
    ; El kernel escribirá la dirección de la GDT en trampoline_gdt_ptr
    lgdt [trampoline_gdt_ptr - trampoline_start + 0x8000]

    ; 2. Pasamos a Modo Protegido
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 3. Far jump a 32-bit (Suponiendo 0x8000 como base)
    jmp 0x08:(protected_mode - trampoline_start + 0x8000)

[bits 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; 4. Configurar Paginación para Long Mode
    ; El kernel escribirá el PML4 en trampoline_pml4
    mov eax, [trampoline_pml4 - trampoline_start + 0x8000]
    mov cr3, eax

    ; 5. Habilitar PAE
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; 6. Habilitar Long Mode en EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; 7. Activar Paginación
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    ; 8. Cargar GDT de 64 bits (proporcionada por el kernel)
    lgdt [trampoline_gdt64_ptr - trampoline_start + 0x8000]

    ; 9. Jump a Long Mode
    jmp 0x08:(long_mode - trampoline_start + 0x8000)

[bits 64]
long_mode:
    ; 10. Configurar Stack y Saltar al C Entry Point
    ; El kernel escribirá estos valores antes de despertar al AP
    mov rsp, [trampoline_stack - trampoline_start + 0x8000]
    mov rax, [trampoline_entry - trampoline_start + 0x8000]
    
    ; Pasar el CPU index como argumento (RDI)
    mov edi, [trampoline_cpu_index - trampoline_start + 0x8000]
    
    jmp rax

align 16
trampoline_gdt_ptr:
    dw 0
    dd 0

align 16
trampoline_gdt64_ptr:
    dw 0
    dq 0

align 8
trampoline_pml4:
    dd 0

align 8
trampoline_stack:
    dq 0

align 8
trampoline_entry:
    dq 0

align 4
trampoline_cpu_index:
    dd 0

trampoline_end:
