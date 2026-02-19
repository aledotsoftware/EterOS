#include "efi.h"

/* ========================================================================= */
/* Configuration & Constants                                                 */
/* ========================================================================= */

#define KERNEL_FILENAME     L"kernel.bin"
#define INITRD_FILENAME     L"initrd.img"
#define KERNEL_LOAD_ADDR    0x10000
#define INITRD_LOAD_ADDR    0x400000
#define MEM_MAP_ADDR        0x5000
#define GDT_ADDR            0x6000  /* Relocated GDT */
#define PAGE_TABLE_ADDR     0x70000
#define STACK_TOP           0x90000
#define BOOT_INFO_ADDR      0xA000

/* Boot Info Structure (Must match include/boot.h) */
typedef struct {
    UINT32 signature;       /* "KBOT" (0x544F424B) */
    UINT32 mem_map_count;
    UINT64 mem_map_addr;
    UINT64 fb_addr;         /* Physical Framebuffer Address */
    UINT32 fb_width;
    UINT32 fb_height;
    UINT32 fb_pitch;        /* Bytes per line */
    UINT32 fb_bpp;          /* Bits per pixel */
    UINT64 initrd_addr;     /* Initrd Physical Address */
    UINT32 initrd_size;     /* Initrd Size in Bytes */
} __attribute__((packed)) boot_info_t;

/* E820 Memory Map Entry (Must match include/boot.h / kernel expectations) */
struct e820_entry {
    UINT64 base_addr;
    UINT64 length;
    UINT32 type;
} __attribute__((packed));

#define E820_USABLE     1
#define E820_RESERVED   2
#define E820_ACPI       3
#define E820_NVS        4
#define E820_BAD        5

/* Page Table Flags */
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITE      (1 << 1)
#define PAGE_USER       (1 << 2)
#define PAGE_HUGE       (1 << 7)

/* Global Variables */
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRS;

/* ========================================================================= */
/* Helper Functions                                                          */
/* ========================================================================= */

void *memcpy(void *dest, const void *src, UINTN n) {
    UINT8 *d = (UINT8 *)dest;
    const UINT8 *s = (const UINT8 *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, UINTN n) {
    UINT8 *p = (UINT8 *)s;
    while (n--) *p++ = (UINT8)c;
    return s;
}

int memcmp(const void *s1, const void *s2, UINTN n) {
    const UINT8 *p1 = (const UINT8 *)s1;
    const UINT8 *p2 = (const UINT8 *)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

UINTN strlen(const CHAR16 *str) {
    UINTN len = 0;
    while (str[len]) len++;
    return len;
}

void print(CHAR16 *str) {
    gST->ConOut->OutputString(gST->ConOut, str);
}

void print_hex(UINT64 val) {
    CHAR16 buf[20];
    CHAR16 hex[] = L"0123456789ABCDEF";
    int i;

    buf[0] = '0';
    buf[1] = 'x';

    for (i = 0; i < 16; i++) {
        buf[17 - i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[18] = 0;
    print(buf);
}

/* ========================================================================= */
/* File Loading                                                              */
/* ========================================================================= */

EFI_STATUS load_file(CHAR16 *filename, VOID **buffer, UINTN *size, UINT64 target_addr) {
    EFI_STATUS status;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID file_guid = EFI_FILE_INFO_ID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *file = NULL;
    EFI_FILE_INFO *info = NULL;
    UINTN info_size = 0;

    /* Locate File System */
    status = gBS->LocateProtocol(&fs_guid, NULL, (void**)&fs);
    if (status != EFI_SUCCESS) return status;

    /* Open Root Volume */
    status = fs->OpenVolume(fs, &root);
    if (status != EFI_SUCCESS) return status;

    /* Open File */
    status = root->Open(root, &file, filename, EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) return status;

    /* Get File Info Size */
    status = file->GetInfo(file, &file_guid, &info_size, NULL);
    if (status == EFI_BUFFER_TOO_SMALL) {
        status = gBS->AllocatePool(EfiLoaderData, info_size, (void**)&info);
        if (status != EFI_SUCCESS) return status;
        status = file->GetInfo(file, &file_guid, &info_size, info);
    }
    if (status != EFI_SUCCESS) return status;

    *size = info->FileSize;

    /* Allocate Buffer */
    /* Try to allocate at target_addr if specified */
    if (target_addr != 0) {
        *buffer = (VOID*)target_addr;
        /* Pages needed */
        UINTN pages = (*size + 4095) / 4096;
        status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, (UINT64*)buffer);

        if (status != EFI_SUCCESS) {
            print(L"  [WARN] Failed to alloc at fixed addr, using any pages.\r\n");
            status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, (UINT64*)buffer);
        }
    } else {
        UINTN pages = (*size + 4095) / 4096;
        status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, (UINT64*)buffer);
    }

    if (status != EFI_SUCCESS) return status;

    /* Read File */
    status = file->Read(file, size, *buffer);

    /* Cleanup */
    file->Close(file);
    root->Close(root);
    if (info) gBS->FreePool(info);

    return status;
}

/* ========================================================================= */
/* Graphics Setup (GOP)                                                      */
/* ========================================================================= */

EFI_STATUS setup_gop(boot_info_t *bi) {
    EFI_STATUS status;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    status = gBS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
    if (status != EFI_SUCCESS) return status;

    /* Find best mode (1024x768 preferred) */
    UINT32 best_mode = gop->Mode->Mode;
    UINT32 mode;

    for (mode = 0; mode < gop->Mode->MaxMode; mode++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN size;
        status = gop->QueryMode(gop, mode, &size, &info);
        if (status == EFI_SUCCESS) {
            if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768) {
                best_mode = mode;
            }
        }
    }

    status = gop->SetMode(gop, best_mode);
    if (status != EFI_SUCCESS) return status;

    /* Fill Boot Info */
    bi->fb_addr = gop->Mode->FrameBufferBase;
    bi->fb_width = gop->Mode->Info->HorizontalResolution;
    bi->fb_height = gop->Mode->Info->VerticalResolution;
    bi->fb_pitch = gop->Mode->Info->PixelsPerScanLine * 4; /* Assumes 32bpp BGR8 */
    bi->fb_bpp = 32;

    return EFI_SUCCESS;
}

/* ========================================================================= */
/* Memory Map Conversion                                                     */
/* ========================================================================= */

void convert_memory_map(UINTN map_size, EFI_MEMORY_DESCRIPTOR *map, UINTN desc_size) {
    UINT32 entry_count = 0;
    struct e820_entry *e820 = (struct e820_entry *)(MEM_MAP_ADDR + 4);

    UINT8 *ptr = (UINT8 *)map;
    UINT8 *end = ptr + map_size;

    while (ptr < end) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)ptr;

        UINT32 type = E820_RESERVED;

        switch (desc->Type) {
            case EfiConventionalMemory:
            case EfiBootServicesCode:
            case EfiBootServicesData:
            case EfiLoaderCode:
            case EfiLoaderData:
                type = E820_USABLE;
                break;
            case EfiACPIReclaimMemory:
                type = E820_ACPI;
                break;
            case EfiACPIMemoryNVS:
                type = E820_NVS;
                break;
            case EfiUnusableMemory:
                type = E820_BAD;
                break;
            default:
                type = E820_RESERVED;
                break;
        }

        /* Add Entry */
        e820[entry_count].base_addr = desc->PhysicalStart;
        e820[entry_count].length = desc->NumberOfPages * 4096;
        e820[entry_count].type = type;

        entry_count++;
        ptr += desc_size;
    }

    /* Write Count */
    *(UINT32 *)MEM_MAP_ADDR = entry_count;
}

/* ========================================================================= */
/* Page Table Setup                                                          */
/* ========================================================================= */

void setup_page_tables() {
    /* Clear 24KB (0x70000 - 0x76000) */
    memset((void*)PAGE_TABLE_ADDR, 0, 0x6000);

    UINT64 *pml4 = (UINT64 *)PAGE_TABLE_ADDR;
    UINT64 *pdpt = (UINT64 *)(PAGE_TABLE_ADDR + 0x1000);
    UINT64 *pd   = (UINT64 *)(PAGE_TABLE_ADDR + 0x2000);

    /* 1. PML4[0] -> PDPT */
    pml4[0] = (UINT64)pdpt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    /* 2. PDPT[0..3] -> PDs */
    /* We map 4GB using 4 PDs */
    for (int i = 0; i < 4; i++) {
        pdpt[i] = ((UINT64)pd + (i * 0x1000)) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    /* 3. Fill PDs with 2MB Huge Pages (Identity Mapping) */
    UINT64 phys = 0;
    UINT64 *current_pd = pd;

    for (int i = 0; i < 2048; i++) { /* 2048 entries * 2MB = 4GB */
        *current_pd = phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_HUGE;
        current_pd++;
        phys += 0x200000; /* +2MB */
    }
}

/* ========================================================================= */
/* Main Entry Point                                                          */
/* ========================================================================= */

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRS = SystemTable->RuntimeServices;

    /* Disable Watchdog */
    gBS->SetWatchdogTimer(0, 0, 0, NULL);

    gST->ConOut->ClearScreen(gST->ConOut);
    print(L"eterOS UEFI Bootloader\r\n");
    print(L"======================\r\n");

    /* Check execution address */
    if ((UINT64)efi_main > 0xFFFFFFFF) {
        print(L"WARNING: Bootloader loaded > 4GB. This might crash during transition.\r\n");
    }

    /* Load Kernel */
    VOID *kernel_buffer = NULL;
    UINTN kernel_size = 0;
    print(L"  Loading Kernel... ");
    if (load_file(KERNEL_FILENAME, &kernel_buffer, &kernel_size, KERNEL_LOAD_ADDR) != EFI_SUCCESS) {
        print(L"FAILED!\r\n");
        return EFI_LOAD_ERROR;
    }
    print(L"OK\r\n");
    print(L"    Addr: "); print_hex((UINT64)kernel_buffer); print(L"\r\n");

    /* Load Initrd */
    VOID *initrd_buffer = NULL;
    UINTN initrd_size = 0;
    print(L"  Loading Initrd... ");
    if (load_file(INITRD_FILENAME, &initrd_buffer, &initrd_size, INITRD_LOAD_ADDR) != EFI_SUCCESS) {
        print(L"FAILED!\r\n");
        return EFI_LOAD_ERROR;
    }
    print(L"OK\r\n");

    /* Setup Graphics */
    boot_info_t bi;
    memset(&bi, 0, sizeof(bi));
    bi.signature = 0x544F424B; /* "KBOT" */

    print(L"  Setting Video Mode... ");
    if (setup_gop(&bi) != EFI_SUCCESS) {
        print(L"FAILED (GOP)!\r\n");
    } else {
        print(L"OK\r\n");
    }

    /* Get Memory Map */
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;

    gBS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 8192; /* Padding */
    gBS->AllocatePool(EfiLoaderData, map_size, (void**)&map);
    gBS->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);

    /* Exit Boot Services */
    if (gBS->ExitBootServices(ImageHandle, map_key) != EFI_SUCCESS) {
        print(L"ERROR: Failed to exit boot services!\r\n");
        while(1);
    }

    /* Disable Interrupts */
    __asm__ volatile("cli");

    /* ===================================================================== */
    /* CRITICAL SECTION: No UEFI Services Available Here                     */
    /* ===================================================================== */

    /* 1. Relocate Kernel (if needed) */
    if ((UINT64)kernel_buffer != KERNEL_LOAD_ADDR) {
        memcpy((void*)KERNEL_LOAD_ADDR, kernel_buffer, kernel_size);
    }

    /* 2. Relocate Initrd (if needed) */
    if ((UINT64)initrd_buffer != INITRD_LOAD_ADDR) {
        memcpy((void*)INITRD_LOAD_ADDR, initrd_buffer, initrd_size);
    }

    /* Update Boot Info with final addresses */
    bi.initrd_addr = INITRD_LOAD_ADDR;
    bi.initrd_size = initrd_size;

    /* 3. Convert Memory Map */
    convert_memory_map(map_size, map, desc_size);

    /* 4. Write Boot Info to 0xA000 */
    memcpy((void*)BOOT_INFO_ADDR, &bi, sizeof(bi));

    /* 5. Setup Page Tables */
    setup_page_tables();

    /* 6. Setup GDT */
    /* We relocate GDT to a known safe low-mem address */
    struct {
        UINT16 limit;
        UINT64 base;
    } __attribute__((packed)) gdt_ptr;

    static UINT64 gdt_data[] = {
        0,                      /* Null */
        0x00209A0000000000,     /* Code64 (Present, Ring0, Code, Exec/Read, Long Mode) */
        0x0000920000000000      /* Data64 (Present, Ring0, Data, Read/Write) */
    };

    /* Copy GDT to safe low memory */
    memcpy((void*)GDT_ADDR, gdt_data, sizeof(gdt_data));

    gdt_ptr.limit = sizeof(gdt_data) - 1;
    gdt_ptr.base = GDT_ADDR;

    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));

    /* 7. Switch to new Page Tables */
    __asm__ volatile("mov %0, %%cr3" : : "r"((UINT64)PAGE_TABLE_ADDR));

    /* 8. Setup Stack & Jump */
    __asm__ volatile (
        "mov $0x90000, %%rsp\n"
        "mov $0x10000, %%rax\n"
        "jmp *%%rax\n"
        : : : "rax", "memory"
    );

    while(1);
    return EFI_SUCCESS;
}
