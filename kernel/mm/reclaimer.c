#include "../../include/mm.h"
#include "../../include/vmm.h"
#include "../../include/pmm.h"
#include "../../include/task.h"
#include "../../include/serial.h"

/* Helper para revisar tablas (inspirado en free_pt_recursive/vmm_clone_pml4) */
typedef uint64_t pt_entry_t;

static void reclaim_pt_recursive(pt_entry_t* table, int level) {
    for (int i = 0; i < 512; i++) {
        if (!(table[i] & PAGE_PRESENT)) continue;

        /* Skip Kernel Mappings */
        if (level == 4 && i != 0) continue;
        if (level == 3 && i < 8) continue;

        if (level > 1) {
            pt_entry_t* child = (pt_entry_t*)(table[i] & PAGE_ADDR_MASK);
            reclaim_pt_recursive(child, level - 1);
        } else {
            /* Level 1 (PT): Evaluar páginas de usuario */
            if (table[i] & PAGE_USER) {
                if (table[i] & PAGE_ACCESSED) {
                    /* Accesada recientemente, limpiamos el bit para darle una 2da chance */
                    table[i] &= ~PAGE_ACCESSED;
                } else {
                    /* No accesible recientemente: Reclamar (LRU eviction) */
                    uint64_t phys = table[i] & PAGE_ADDR_MASK;
                    
                    /* Si la página no está ya swappeada */
                    if (!(table[i] & PAGE_SWAPPED)) {
                        /* Simular swap-out liberando la página y dejando un bit marcador */
                        pmm_unref_page((void*)phys);
                        
                        /* Mantener los demás flags pero quitar PAGE_PRESENT y agregar PAGE_SWAPPED */
                        table[i] &= ~PAGE_PRESENT;
                        table[i] |= PAGE_SWAPPED;
                    }
                }
            }
        }
    }
}

static void page_reclaimer_thread(void) {
    serial_write_string("[RECLAIMER] Thread started.\n");
    while (1) {
        /* Dormir 5 segundos (o lo que defina el kernel) antes del próximo ciclo de limpieza */
        task_sleep(5000);

        /* Si hay poca memoria (menos de 20% o valor fijo), intentamos reclamar */
        uint64_t total = pmm_get_total_ram();
        uint64_t free = pmm_get_free_ram();
        
        if (free < (total / 5)) {
            serial_write_string("[RECLAIMER] Low memory detected. Scanning for pages...\n");
            
            /* Recorrer todas las tareas y sus PML4 */
            for (int i = 0; i < task_get_max(); i++) {
                task_t* t = task_get_at(i);
                if (!t || t->state == 0 || t->state == TASK_DEAD) continue;

                /* No reclamar desde el CR3 del kernel u ocioso */
                if (t->id == 0) continue; 

                uint64_t cr3 = t->cr3;
                if (!cr3) continue;

                pt_entry_t* pml4 = (pt_entry_t*)(cr3 & PAGE_ADDR_MASK);
                reclaim_pt_recursive(pml4, 4);
                
                /* Flush TLB para seguridad si estuviéramos en la misma CPU, pero siendo conservadores:
                   recarga CR3 de esa tarea no es posible porque corre, el IPI Shootdown 
                   se usa para direcciones virtuales específicas. TODO: full shootdown. */
            }
        }
    }
}

void reclaimer_init(void) {
    task_create("reclaimer", page_reclaimer_thread);
}
