/**
 * éterOS - Simple Memory Allocator (First-Fit + Coalescing)
 * 
 * Implementación de un gestor de memoria dinámica simple.
 * Usa una lista enlazada de bloques libres y ocupados.
 * El heap vive en la región identity-mapped del bootloader (0-4GB).
 */

#include "../../include/types.h"
#include "../../include/string.h"
#include "../../include/mm.h"
#include "../../include/serial.h"

/* ========================================================================= */
/* Configuración del Heap                                                    */
/* ========================================================================= */

/* Dirección física fija para el Heap: 4MB (dentro del identity map 0-4GB) */
#define HEAP_START_ADDR  ((void*)0x00400000)
#define HEAP_SIZE        ((size_t)(8 * 1024 * 1024)) /* 8 MB */
#define HEAP_ALIGNMENT   8

/* Estructura de cabecera de bloque */
typedef struct block_header {
    size_t size;
    uint8_t is_free;
    struct block_header* next;
} block_header_t;

static block_header_t* heap_start = (block_header_t*)HEAP_START_ADDR;
static size_t memory_used = 0;
static size_t memory_total = HEAP_SIZE;

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

static size_t align(size_t n) {
    return (n + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
}

static void coalesce(void) {
    block_header_t* curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

/* ========================================================================= */
/* API Pública                                                               */
/* ========================================================================= */

void mm_init(void) {
    heap_start->size = HEAP_SIZE - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    memory_used = 0;
    serial_write_string("[MM] Heap inicializado en 0x00400000 (8 MB)\n");
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t aligned_size = align(size);
    block_header_t* curr = heap_start;
    
    while (curr) {
        if (curr->is_free && curr->size >= aligned_size) {
            if (curr->size >= aligned_size + sizeof(block_header_t) + HEAP_ALIGNMENT) {
                block_header_t* new_block = (block_header_t*)((uintptr_t)curr + 
                                            sizeof(block_header_t) + aligned_size);
                new_block->size = curr->size - aligned_size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;
                curr->size = aligned_size;
                curr->next = new_block;
            }
            
            curr->is_free = 0;
            memory_used += curr->size + sizeof(block_header_t);
            return (void*)((uintptr_t)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }
    
    serial_write_string("[MM] CRITICAL: Out of Memory!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    if ((void*)block < HEAP_START_ADDR || (void*)block >= (void*)((uintptr_t)HEAP_START_ADDR + HEAP_SIZE)) {
        serial_write_string("[MM] Error: kfree of invalid address\n");
        return;
    }

    if (block->is_free) {
        serial_write_string("[MM] Warning: Double free detected\n");
        return;
    }
    
    block->is_free = 1;
    memory_used -= (block->size + sizeof(block_header_t));
    coalesce();
}

void* kcalloc(size_t num, size_t size) {
    if (num > 0 && size > SIZE_MAX / num) return NULL;
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

size_t mm_get_total_memory(void) { return memory_total; }
size_t mm_get_used_memory(void) { return memory_used; }
