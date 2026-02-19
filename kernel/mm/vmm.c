/**
 * éterOS - Virtual Memory Manager (VMM)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * 
 * Gestiona la paginación de 4 niveles (x86_64 Long Mode).
 * Controla PML4, PDPT, PD, PT para mapear direcciones virtuales a físicas.
 */

#include "../../include/vmm.h"
#include "../../include/pmm.h"
#include "../../include/string.h"
#include "../../include/serial.h"
#include "../../include/hal/mm.h"

/* Estructura de una entrada de tabla de páginas (64-bit) */
typedef uint64_t pt_entry_t;

/* Puntero a la tabla PML4 activa */
/* static pt_entry_t* pml4 = (pt_entry_t*)BOOT_PML4_ADDR; */
/* NOTA: Ya no usamos la variable estática 'pml4' porque puede estar desactualizada.
   Usamos CR3 directamente. */

/* Invalida una página en el TLB */
static inline void invlpg(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

/* Recarga CR3 (flush completo de TLB - costoso) */
void vmm_load_pml4(uint64_t pml4_addr) {
    __asm__ volatile("mov %0, %%cr3" : : "r" (pml4_addr) : "memory");
}

uint64_t vmm_get_pml4(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/*
 * Obtiene o crea la siguiente tabla en la jerarquía.
 * Si la entrada no existe, asigna una nueva página física del PMM,
 * la limpia, y la enlaza en la tabla actual.
 */
static pt_entry_t* get_next_table(pt_entry_t* table, uint64_t index, int alloc) {
    if (table[index] & PAGE_PRESENT) {
        /* Check for Huge Page (Identity Mapped by Bootloader) */
        if (table[index] & PAGE_HUGE) {
             serial_write_string("[VMM] Error: Cannot traverse Huge Page\n");
             return NULL;
        }
    
        /* La tabla existe, devolver su dirección virtual (Identity Mapping) */
        /* NOTA: Como usamos Identity Mapping para el kernel, Physical == Virtual */
        return (pt_entry_t*)(table[index] & PAGE_ADDR_MASK);
    }

    if (!alloc) return NULL;

    /* No existe, crear nueva tabla */
    void* new_table_phys = pmm_alloc_page();
    if (!new_table_phys) {
        serial_write_string("[VMM] Error: OOM, no se pudo alocar tabla de paginacion\n");
        return NULL;
    }

    /* Limpiar la nueva tabla (crítico para evitar entradas basura) */
    memset(new_table_phys, 0, PAGE_SIZE);

    /* Enlazar en la tabla padre (Present + Reserved/User/RW flags) */
    /* Heredamos permisos permisivos para las tablas intermedias, restringimos en la hoja (PT) */
    table[index] = (uint64_t)new_table_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    return (pt_entry_t*)new_table_phys;
}

void vmm_init(void) {
    serial_write_string("[VMM] Inicializando Gestor de Memoria Virtual...\n");
    serial_write_string("[VMM] Usando PML4 actual (CR3)\n");
}

int vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags) {
    /* Navegar la jerarquía: PML4 -> PDPT -> PD -> PT */
    
    pt_entry_t* pml4 = (pt_entry_t*)(vmm_get_pml4() & PAGE_ADDR_MASK);

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 1);
    if (!pdpt) return -1;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 1);
    if (!pd) return -1;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 1);
    if (!pt) return -1;

    /* Configurar la entrada de la tabla de páginas (PT Entry) */
    pt[pt_idx] = (phys_addr & PAGE_ADDR_MASK) | flags;

    /* Invalidar TLB para esta dirección */
    invlpg(virt_addr);

    return 0;
}

void vmm_unmap_page(uint64_t virt_addr) {
    pt_entry_t* pml4 = (pt_entry_t*)(vmm_get_pml4() & PAGE_ADDR_MASK);

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
    if (!pdpt) return;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
    if (!pd) return;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
    if (!pt) return;

    /* Marcar como no presente */
    pt[pt_idx] = 0;
    
    invlpg(virt_addr);
}

uint64_t vmm_virt_to_phys(uint64_t virt_addr) {
    pt_entry_t* pml4 = (pt_entry_t*)(vmm_get_pml4() & PAGE_ADDR_MASK);

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
    if (!pdpt) return 0;

    pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
    if (!pd) return 0;

    pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
    if (!pt) return 0;
    
    /* Verificar si la página está presente */
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    
    return (pt[pt_idx] & PAGE_ADDR_MASK) + (virt_addr & 0xFFF);
}

/* ========================================================================= */
/* HAL Implementation                                                        */
/* ========================================================================= */

int hal_mem_map(uint64_t phys_addr, uint64_t virt_addr, uint32_t flags) {
    uint64_t arch_flags = PAGE_PRESENT;

    /* Translate HAL flags to x86_64 Paging flags */
    if (flags & HAL_MEM_WRITE) {
        arch_flags |= PAGE_WRITE;
    }
    if (flags & HAL_MEM_USER) {
        arch_flags |= PAGE_USER;
    }
    if (flags & HAL_MEM_CACHE_DISABLE) {
        arch_flags |= 0x10;
    }

    return vmm_map_page(phys_addr, virt_addr, arch_flags);
}

int hal_mem_unmap(uint64_t virt_addr) {
    vmm_unmap_page(virt_addr);
    return 0;
}

uint64_t hal_mem_get_phys(uint64_t virt_addr) {
    return vmm_virt_to_phys(virt_addr);
}

static void clone_pt_recursive(pt_entry_t* dest, pt_entry_t* src, int level, int cow) {
    for (int i = 0; i < 512; i++) {
        if (!(src[i] & PAGE_PRESENT)) continue;

        /* Check for Shared Kernel Regions */
        /* Level 4 (PML4), Index != 0 -> Shared Kernel (Higher Half) */
        if (level == 4 && i != 0) {
            dest[i] = src[i];
            continue;
        }
        /* Level 3 (PDPT inside PML4[0]), Index < 8 -> Shared Kernel (0-8GB Identity) */
        if (level == 3 && i < 8) {
             dest[i] = src[i];
             continue;
        }

        if (level > 1) {
            /* Alloc new table for next level */
            void* new_table = pmm_alloc_page();
            if (!new_table) return; /* OOM */
            memset(new_table, 0, PAGE_SIZE);

            /* Recurse */
            pt_entry_t* child_src = (pt_entry_t*)(src[i] & PAGE_ADDR_MASK);
            pt_entry_t* child_dest = (pt_entry_t*)new_table;

            clone_pt_recursive(child_dest, child_src, level - 1, cow);

            /* Link new table */
            dest[i] = (uint64_t)new_table | (src[i] & 0xFFF);
        } else {
            /* Level 1 (PT) -> Clone Page */
            uint64_t phys = src[i] & PAGE_ADDR_MASK;
            uint64_t flags = src[i] & ~PAGE_ADDR_MASK;

            /* Ignore non-user pages found in user region (shouldn't happen but safety) */
            if (!(flags & PAGE_USER)) {
                dest[i] = src[i];
                continue;
            }

            if (cow) {
                /* CoW Logic */
                /* Update Source: Read-Only, CoW */
                src[i] |= PAGE_COW;
                src[i] &= ~PAGE_WRITE;

                /* Dest: Same phys, same flags */
                dest[i] = src[i];

                /* Increment Ref */
                pmm_ref_page((void*)phys);
            } else {
                /* Deep Copy */
                void* new_page = pmm_alloc_page();
                if (new_page) {
                    memcpy(new_page, (void*)phys, PAGE_SIZE);
                    dest[i] = (uint64_t)new_page | flags;
                }
            }
        }
    }
}

uint64_t vmm_clone_pml4(int cow) {
    pt_entry_t* new_pml4 = (pt_entry_t*)pmm_alloc_page();
    if (!new_pml4) return 0;
    memset(new_pml4, 0, PAGE_SIZE);

    /* Use current PML4 as source */
    pt_entry_t* current_pml4 = (pt_entry_t*)(vmm_get_pml4() & PAGE_ADDR_MASK);

    /* Start recursion from Level 4 */
    clone_pt_recursive(new_pml4, current_pml4, 4, cow);

    /* If we modified current tables (CoW), we must flush TLB */
    if (cow) {
        uint64_t current_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3) : "memory");
    }

    return (uint64_t)new_pml4;
}

uint64_t vmm_create_pml4(void) {
    pt_entry_t* new_pml4 = (pt_entry_t*)pmm_alloc_page();
    if (!new_pml4) return 0;
    memset(new_pml4, 0, PAGE_SIZE);

    pt_entry_t* boot_pml4 = (pt_entry_t*)BOOT_PML4_ADDR;

    /* Copy Higher Half Kernel (256-511) */
    for (int i = 256; i < 512; i++) {
         new_pml4[i] = boot_pml4[i];
    }

    /* Identity Map (0-4MB or 0-8GB) resides in pml4[0] -> pdpt[0..7] */
    /* We MUST allocate a new PDPT for index 0 to separate User space (index >= 8) */
    pt_entry_t* new_pdpt = (pt_entry_t*)pmm_alloc_page();
    if (!new_pdpt) {
         pmm_free_page(new_pml4);
         return 0;
    }
    memset(new_pdpt, 0, PAGE_SIZE);

    /* Link new PDPT to new PML4[0] */
    new_pml4[0] = (uint64_t)new_pdpt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    /* Copy Kernel Identity Map (indices 0-7 of PDPT) */
    if (boot_pml4[0] & PAGE_PRESENT) {
         pt_entry_t* boot_pdpt = (pt_entry_t*)(boot_pml4[0] & PAGE_ADDR_MASK);
         for (int i = 0; i < 8; i++) {
             new_pdpt[i] = boot_pdpt[i];
         }
    }

    return (uint64_t)new_pml4;
}

static void destroy_level(pt_entry_t* table, int level) {
    for (int i = 0; i < 512; i++) {
         if (!(table[i] & PAGE_PRESENT)) continue;

         /* Skip Kernel Mappings */
         if (level == 4 && i >= 256) continue; /* Higher Half */
         if (level == 3 && i < 8) continue;    /* Identity Map */

         uint64_t child_phys = table[i] & PAGE_ADDR_MASK;
         pt_entry_t* child = (pt_entry_t*)child_phys;

         if (level > 1) {
             destroy_level(child, level - 1);
             /* Free the table page */
             pmm_free_page(child);
         } else {
             /* Level 1: Page Table. Entries are pages. */
             /* Check PAGE_USER bit just in case */
             if (table[i] & PAGE_USER) {
                 pmm_free_page((void*)child_phys);
             }
         }
    }
}

void vmm_destroy_pml4(uint64_t pml4_phys) {
     pt_entry_t* pml4 = (pt_entry_t*)pml4_phys;
     destroy_level(pml4, 4);
     pmm_free_page(pml4);
}

int vmm_handle_page_fault(uint64_t addr, uint64_t error_code) {
    /* Error Code:
       Bit 0: Present (0=Not Present, 1=Protection Violation)
       Bit 1: Write (1=Write)
       Bit 2: User (1=User)
    */

    int present = error_code & 1;
    int write   = error_code & 2;
    int user    = error_code & 4;

    /* Handle CoW: Write + Present + User */
    if (present && write && user) {
        /* Walk to find the entry */
        pt_entry_t* pml4 = (pt_entry_t*)(vmm_get_pml4() & PAGE_ADDR_MASK);

        uint64_t pml4_idx = PML4_INDEX(addr);
        uint64_t pdpt_idx = PDPT_INDEX(addr);
        uint64_t pd_idx   = PD_INDEX(addr);
        uint64_t pt_idx   = PT_INDEX(addr);

        pt_entry_t* pdpt = get_next_table(pml4, pml4_idx, 0);
        if (!pdpt) return 0;
        pt_entry_t* pd = get_next_table(pdpt, pdpt_idx, 0);
        if (!pd) return 0;
        pt_entry_t* pt = get_next_table(pd, pd_idx, 0);
        if (!pt) return 0;

        if (pt[pt_idx] & PAGE_COW) {
             /* CoW Fault! */
             uint64_t old_phys = pt[pt_idx] & PAGE_ADDR_MASK;
             uint64_t flags = pt[pt_idx] & ~PAGE_ADDR_MASK;

             /* Check refcount */
             if (pmm_get_ref_count((void*)old_phys) == 1) {
                 /* Optimize: If refcount is 1, just make it writable again (we are the last owner) */
                 pt[pt_idx] &= ~PAGE_COW;
                 pt[pt_idx] |= PAGE_WRITE;
             } else {
                 /* Alloc new page */
                 void* new_phys = pmm_alloc_page();
                 if (!new_phys) {
                     serial_write_string("[VMM] OOM during CoW\n");
                     return 0;
                 }

                 /* Copy content */
                 memcpy(new_phys, (void*)old_phys, PAGE_SIZE);

                 /* Remap */
                 pt[pt_idx] = (uint64_t)new_phys | (flags & ~PAGE_COW) | PAGE_WRITE;

                 /* Decrement old ref */
                 pmm_unref_page((void*)old_phys);
             }

             invlpg(addr);
             return 1; /* Handled */
        }
    }
    return 0; /* Not handled */
}

int vmm_is_user_page(uint64_t virt_addr) {
    pt_entry_t* pml4 = (pt_entry_t*)(vmm_get_pml4() & PAGE_ADDR_MASK);

    uint64_t pml4_idx = PML4_INDEX(virt_addr);
    uint64_t pdpt_idx = PDPT_INDEX(virt_addr);
    uint64_t pd_idx   = PD_INDEX(virt_addr);
    uint64_t pt_idx   = PT_INDEX(virt_addr);

    /* Level 4: PML4 */
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    if (!(pml4[pml4_idx] & PAGE_USER)) return 0;

    pt_entry_t* pdpt = (pt_entry_t*)(pml4[pml4_idx] & PAGE_ADDR_MASK);

    /* Level 3: PDPT */
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    if (!(pdpt[pdpt_idx] & PAGE_USER)) return 0;

    if (pdpt[pdpt_idx] & PAGE_HUGE) {
        /* 1GB Page - User bit is sufficient */
        return 1;
    }

    pt_entry_t* pd = (pt_entry_t*)(pdpt[pdpt_idx] & PAGE_ADDR_MASK);

    /* Level 2: PD */
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    if (!(pd[pd_idx] & PAGE_USER)) return 0;

    if (pd[pd_idx] & PAGE_HUGE) {
        /* 2MB Page - User bit is sufficient */
        return 1;
    }

    pt_entry_t* pt = (pt_entry_t*)(pd[pd_idx] & PAGE_ADDR_MASK);

    /* Level 1: PT */
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    if (!(pt[pt_idx] & PAGE_USER)) return 0;

    return 1;
}
