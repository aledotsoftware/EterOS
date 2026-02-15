#ifndef ACPI_H
#define ACPI_H

#include <types.h>

/* Firmas de tablas */
#define ACPI_RSDP_SIG "RSD PTR "
#define ACPI_MADT_SIG "APIC"

/* Estructura RSDP (Root System Description Pointer) */
/* Se busca en 0xE0000 - 0xFFFFF o en EBDA */
typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;     /* Dirección física de RSDT (32-bit) */
    
    /* ACPI 2.0+ fields */
    uint32_t length;
    uint64_t xsdt_addr;     /* Dirección física de XSDT (64-bit) */
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

/* Header genérico para todas las tablas (RSDT, MADT, etc.) */
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

/* RSDT (Root System Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t tables[];      /* Punteros a otras tablas */
} __attribute__((packed)) acpi_rsdt_t;

/* MADT (Multiple APIC Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t lapic_addr;    /* Dirección física del Local APIC */
    uint32_t flags;         /* Bit 0 = PCAT_COMPAT (Dual 8259 present) */
    uint8_t entries[];      /* Entradas de longitud variable */
} __attribute__((packed)) acpi_madt_t;

/* Tipos de entradas en MADT */
#define MADT_ENTRY_LAPIC      0
#define MADT_ENTRY_IOAPIC     1
#define MADT_ENTRY_ISO        2
#define MADT_ENTRY_NMI        4

/* Estructura genérica de entrada MADT */
typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

/* Processor Local APIC Entry */
typedef struct {
    madt_entry_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;        /* El ID real del hardware para interrupciones */
    uint32_t flags;         /* Bit 0 = Processor Enabled */
} __attribute__((packed)) madt_lapic_t;

/* I/O APIC Entry */
typedef struct {
    madt_entry_header_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t global_system_interrupt_base;
} __attribute__((packed)) madt_ioapic_t;

/* Funciones Públicas */
void acpi_init(void);
int acpi_get_cpu_count(void);

#endif
