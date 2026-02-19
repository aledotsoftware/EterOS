#include <fs/fat32.h>
#include <string.h>
#include <hal.h>
#include <mm.h>

#define FAT32_EOC 0x0FFFFFF8

/* Helper: Convert 8.3 name from entry to normal string */
static void fat32_get_name(const uint8_t* entry_name, char* out_name) {
    int i, j = 0;
    // Copy Name
    for (i = 0; i < 8; i++) {
        if (entry_name[i] == ' ') break;
        out_name[j++] = entry_name[i];
    }
    // Copy Extension
    if (entry_name[8] != ' ') {
        out_name[j++] = '.';
        for (i = 8; i < 11; i++) {
            if (entry_name[i] == ' ') break;
            out_name[j++] = entry_name[i];
        }
    }
    out_name[j] = '\0';
}

/* Helper: Convert input name to 8.3 format for comparison */
static void fat32_to_dos_name(const char* name, char* dos_name) {
    memset(dos_name, ' ', 11);
    int i = 0, j = 0;

    // Copy name part
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32; // To Upper
        dos_name[j++] = c;
        i++;
    }

    // Skip to dot or end
    while (name[i] && name[i] != '.') i++;

    if (name[i] == '.') {
        i++;
        j = 8;
        // Copy extension
        while (name[i] && j < 11) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32; // To Upper
            dos_name[j++] = c;
            i++;
        }
    }
}

int fat32_init(fat32_volume_t* vol, fat32_read_sector_t read_func, fat32_write_sector_t write_func, uint32_t partition_offset) {
    if (!vol || !read_func) return -1;

    vol->read_sector = read_func;
    vol->write_sector = write_func;
    vol->partition_lba = partition_offset;

    // Allocate 4096 to be safe for larger sector sizes during init
    uint8_t* buffer = kmalloc(4096);
    if (!buffer) return -2;

    // Read Boot Sector
    if (vol->read_sector(vol->partition_lba, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    fat32_boot_sector_t* bpb = (fat32_boot_sector_t*)buffer;

    // Check Signature
    if (bpb->boot_sector_signature != 0xAA55) {
        hal_console_write("[FAT32] Invalid Boot Sector Signature\n");
        kfree(buffer);
        return -4;
    }

    // Load geometry
    vol->bytes_per_sector = bpb->bytes_per_sector;
    vol->sectors_per_cluster = bpb->sectors_per_cluster;
    vol->root_cluster = bpb->root_cluster;

    // Check minimal sector size (usually 512)
    if (vol->bytes_per_sector == 0) {
        kfree(buffer);
        return -5;
    }

    // Calculate offsets
    uint32_t fat_size = bpb->fat_size_32; // FAT32 uses fat_size_32
    if (fat_size == 0) fat_size = bpb->fat_size_16; // Fallback, though FAT32 should use 32

    vol->fat_size = fat_size;
    vol->fat_start_lba = vol->partition_lba + bpb->reserved_sectors;

    // Data Start = Partition + Reserved + (NumFATs * FATSize)
    // Note: Root Directory in FAT32 is a cluster chain, usually starting at cluster 2 (start of data area).
    vol->data_start_lba = vol->fat_start_lba + (bpb->num_fats * fat_size);

    kfree(buffer);
    return 0;
}

static uint32_t fat32_get_next_cluster(fat32_volume_t* vol, uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return 0xFFFFFFFF;

    if (vol->read_sector(fat_sector, buffer) != 0) {
        kfree(buffer);
        return 0xFFFFFFFF;
    }

    uint32_t next_cluster = *(uint32_t*)&buffer[ent_offset];
    kfree(buffer);

    return next_cluster & 0x0FFFFFFF;
}

static uint32_t fat32_cluster_to_lba(fat32_volume_t* vol, uint32_t cluster) {
    return vol->data_start_lba + ((cluster - 2) * vol->sectors_per_cluster);
}

/* Helper: Write to FAT Table */
static int fat32_fat_set(fat32_volume_t* vol, uint32_t cluster, uint32_t value) {
    if (!vol->write_sector) return -1;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t ent_offset = fat_offset % vol->bytes_per_sector;

    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -2;

    // Read sector first (RMW)
    if (vol->read_sector(fat_sector, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    // Update entry
    *(uint32_t*)&buffer[ent_offset] = value;

    // Write back
    if (vol->write_sector(fat_sector, buffer) != 0) {
        kfree(buffer);
        return -4;
    }

    kfree(buffer);
    return 0;
}

/* Helper: Allocate a free cluster */
static int fat32_alloc_cluster(fat32_volume_t* vol, uint32_t* out_cluster) {
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    uint32_t current_fat_sector = vol->fat_start_lba;
    uint32_t entries_per_sector = vol->bytes_per_sector / 4;
    uint32_t cluster_idx = 0;

    // Scan full FAT
    for (uint32_t i = 0; i < vol->fat_size; i++) {
        if (vol->read_sector(current_fat_sector + i, buffer) != 0) {
            kfree(buffer);
            return -2;
        }

        uint32_t* entries = (uint32_t*)buffer;
        for (uint32_t j = 0; j < entries_per_sector; j++) {
            // Cluster 0 and 1 are reserved.
            // But we need to be careful with indexing.
            // If i=0, j=0 => cluster 0. j=1 => cluster 1.
            // We should start checking from cluster 2.
            // Current cluster_idx counts absolute cluster number.

            if (cluster_idx < 2) {
                cluster_idx++;
                continue;
            }

            if ((entries[j] & 0x0FFFFFFF) == 0) {
                // Found free
                *out_cluster = cluster_idx;

                // Mark as EOC
                entries[j] = FAT32_EOC;
                if (vol->write_sector(current_fat_sector + i, buffer) != 0) {
                    kfree(buffer);
                    return -3;
                }

                // Zero out the cluster data
                uint8_t* zero_buf = kmalloc(vol->bytes_per_sector);
                if (zero_buf) {
                    memset(zero_buf, 0, vol->bytes_per_sector);
                    uint32_t lba = fat32_cluster_to_lba(vol, *out_cluster);
                    for (uint32_t k = 0; k < vol->sectors_per_cluster; k++) {
                        vol->write_sector(lba + k, zero_buf);
                    }
                    kfree(zero_buf);
                }

                kfree(buffer);
                return 0;
            }
            cluster_idx++;
        }
    }

    kfree(buffer);
    return -4; // No free space found
}

/* Helper: Free cluster chain */
static void fat32_free_cluster_chain(fat32_volume_t* vol, uint32_t start_cluster) {
    uint32_t current = start_cluster;
    while (current >= 2 && current < FAT32_EOC) {
        uint32_t next = fat32_get_next_cluster(vol, current);
        fat32_fat_set(vol, current, 0); // Mark free
        current = next;
    }
}

/* Helper: Find a free directory entry slot in Root Directory */
static int fat32_find_free_dirent(fat32_volume_t* vol, uint32_t* out_sector, uint32_t* out_offset) {
    uint32_t current_cluster = vol->root_cluster;
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    while (current_cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (vol->read_sector(lba + i, buffer) != 0) {
                kfree(buffer);
                return -2;
            }

            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)buffer;
            int entries_per_sector = vol->bytes_per_sector / sizeof(fat32_dir_entry_t);

            for (int j = 0; j < entries_per_sector; j++) {
                if (entry[j].name[0] == DIRENT_END || entry[j].name[0] == DIRENT_DELETED) {
                    *out_sector = lba + i;
                    *out_offset = j * sizeof(fat32_dir_entry_t);
                    kfree(buffer);
                    return 0;
                }
            }
        }

        uint32_t next = fat32_get_next_cluster(vol, current_cluster);
        if (next >= FAT32_EOC) {
            // Extend directory
            uint32_t new_cluster;
            if (fat32_alloc_cluster(vol, &new_cluster) == 0) {
                fat32_fat_set(vol, current_cluster, new_cluster);
                current_cluster = new_cluster;
            } else {
                kfree(buffer);
                return -3; // Disk full
            }
        } else {
            current_cluster = next;
        }
    }
    kfree(buffer);
    return -4;
}

/* Helper: Update or Create Directory Entry */
static int fat32_update_dirent(fat32_volume_t* vol, uint32_t sector, uint32_t offset, fat32_dir_entry_t* value) {
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    if (vol->read_sector(sector, buffer) != 0) {
        kfree(buffer);
        return -2;
    }

    memcpy(buffer + offset, value, sizeof(fat32_dir_entry_t));

    if (vol->write_sector(sector, buffer) != 0) {
        kfree(buffer);
        return -3;
    }

    kfree(buffer);
    return 0;
}

/* Helper: Get Directory Entry Location */
static int fat32_get_dirent_loc(fat32_volume_t* vol, const char* filename, uint32_t* out_sector, uint32_t* out_offset, fat32_dir_entry_t* out_entry) {
    char dos_name[11];
    fat32_to_dos_name(filename, dos_name);

    uint32_t current_cluster = vol->root_cluster;
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -1;

    while (current_cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (vol->read_sector(lba + i, buffer) != 0) {
                kfree(buffer);
                return -2;
            }

            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)buffer;
            int entries_per_sector = vol->bytes_per_sector / sizeof(fat32_dir_entry_t);

            for (int j = 0; j < entries_per_sector; j++) {
                if (entry[j].name[0] == DIRENT_END) {
                    kfree(buffer);
                    return -3; // Not found
                }
                if (entry[j].name[0] == DIRENT_DELETED) continue;
                if (entry[j].attr & ATTR_VOLUME_ID) continue;
                if (entry[j].attr & ATTR_LONG_NAME) continue;

                if (memcmp(entry[j].name, dos_name, 11) == 0) {
                    if (out_entry) *out_entry = entry[j];
                    if (out_sector) *out_sector = lba + i;
                    if (out_offset) *out_offset = j * sizeof(fat32_dir_entry_t);
                    kfree(buffer);
                    return 0;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
    }
    kfree(buffer);
    return -3;
}

int fat32_create_file(fat32_volume_t* vol, const char* filename) {
    if (fat32_find_file(vol, filename, NULL) == 0) return -1; // Exists

    uint32_t sector, offset;
    if (fat32_find_free_dirent(vol, &sector, &offset) != 0) return -2; // No space

    fat32_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    fat32_to_dos_name(filename, (char*)entry.name);
    entry.attr = ATTR_ARCHIVE;
    entry.file_size = 0;
    // We can allocate cluster later or now. Let's start with empty (cluster 0)
    entry.fst_clus_hi = 0;
    entry.fst_clus_lo = 0;

    return fat32_update_dirent(vol, sector, offset, &entry);
}

int fat32_write_file(fat32_volume_t* vol, const char* filename, const void* buffer, uint32_t size) {
    fat32_dir_entry_t entry;
    uint32_t dir_sector, dir_offset;
    if (fat32_get_dirent_loc(vol, filename, &dir_sector, &dir_offset, &entry) != 0) {
        // Create if not exists?
        // Prompt implies writing, maybe we should auto-create?
        // Standard "write" usually writes to existing file descriptor.
        // But let's assume strict separation as per "create_file" existence.
        // Actually, let's auto-create if not found for convenience, or error.
        // Error is safer.
        return -1;
    }

    if (entry.attr & ATTR_DIRECTORY) return -2;

    uint32_t cluster = (uint32_t)entry.fst_clus_hi << 16 | entry.fst_clus_lo;

    // Handle 0-size write: Truncate file
    if (size == 0) {
        if (cluster != 0) {
            fat32_free_cluster_chain(vol, cluster);
        }
        entry.file_size = 0;
        entry.fst_clus_hi = 0;
        entry.fst_clus_lo = 0;
        return fat32_update_dirent(vol, dir_sector, dir_offset, &entry);
    }

    // Check if we need to allocate initial cluster
    if (cluster == 0) {
        if (fat32_alloc_cluster(vol, &cluster) != 0) return -3;
        entry.fst_clus_hi = (cluster >> 16) & 0xFFFF;
        entry.fst_clus_lo = cluster & 0xFFFF;
    }

    uint32_t bytes_written = 0;
    const uint8_t* in_ptr = (const uint8_t*)buffer;
    uint32_t current_cluster = cluster;
    uint32_t prev_cluster = 0; // To link chain if extended

    uint8_t* sector_buffer = kmalloc(vol->bytes_per_sector);
    if (!sector_buffer) return -4;

    while (bytes_written < size) {
        if (current_cluster == 0 || current_cluster >= FAT32_EOC) {
            // Need new cluster
            uint32_t new_cluster;
            if (fat32_alloc_cluster(vol, &new_cluster) != 0) {
                kfree(sector_buffer);
                return -5;
            }
            if (prev_cluster != 0) {
                fat32_fat_set(vol, prev_cluster, new_cluster);
            }
            current_cluster = new_cluster;
        }

        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);
        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (bytes_written >= size) break;

            uint32_t chunk = vol->bytes_per_sector;
            if (bytes_written + chunk > size) {
                chunk = size - bytes_written;
            }

            memset(sector_buffer, 0, vol->bytes_per_sector);
            memcpy(sector_buffer, in_ptr + bytes_written, chunk);

            if (vol->write_sector(lba + i, sector_buffer) != 0) {
                kfree(sector_buffer);
                return -6;
            }
            bytes_written += chunk;
        }

        prev_cluster = current_cluster;
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
    }

    // Update file size
    entry.file_size = size;
    fat32_update_dirent(vol, dir_sector, dir_offset, &entry);

    // Terminate chain
    if (prev_cluster != 0) {
        fat32_fat_set(vol, prev_cluster, FAT32_EOC);

        // Free remaining if any
        if (current_cluster >= 2 && current_cluster < FAT32_EOC) {
            fat32_free_cluster_chain(vol, current_cluster);
        }
    }

    kfree(sector_buffer);
    return bytes_written;
}

int fat32_delete_file(fat32_volume_t* vol, const char* filename) {
    fat32_dir_entry_t entry;
    uint32_t dir_sector, dir_offset;
    if (fat32_get_dirent_loc(vol, filename, &dir_sector, &dir_offset, &entry) != 0) return -1;

    if (entry.attr & ATTR_DIRECTORY) return -2; // Cannot delete directory with delete_file

    // Free cluster chain
    uint32_t cluster = (uint32_t)entry.fst_clus_hi << 16 | entry.fst_clus_lo;
    if (cluster != 0) {
        fat32_free_cluster_chain(vol, cluster);
    }

    // Mark deleted
    entry.name[0] = DIRENT_DELETED;
    return fat32_update_dirent(vol, dir_sector, dir_offset, &entry);
}

int fat32_create_directory(fat32_volume_t* vol, const char* dirname) {
    if (fat32_find_file(vol, dirname, NULL) == 0) return -1; // Exists

    uint32_t sector, offset;
    if (fat32_find_free_dirent(vol, &sector, &offset) != 0) return -2;

    // Alloc cluster for directory content
    uint32_t cluster;
    if (fat32_alloc_cluster(vol, &cluster) != 0) return -3;

    // Create entry
    fat32_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    fat32_to_dos_name(dirname, (char*)entry.name);
    entry.attr = ATTR_DIRECTORY;
    entry.fst_clus_hi = (cluster >> 16) & 0xFFFF;
    entry.fst_clus_lo = cluster & 0xFFFF;
    entry.file_size = 0; // Dir size is usually 0

    if (fat32_update_dirent(vol, sector, offset, &entry) != 0) return -4;

    // Initialize . and ..
    fat32_dir_entry_t dot[2];
    memset(dot, 0, sizeof(dot));

    // .
    memset(dot[0].name, ' ', 11);
    dot[0].name[0] = '.';
    dot[0].attr = ATTR_DIRECTORY;
    dot[0].fst_clus_hi = entry.fst_clus_hi;
    dot[0].fst_clus_lo = entry.fst_clus_lo;

    // ..
    memset(dot[1].name, ' ', 11);
    dot[1].name[0] = '.';
    dot[1].name[1] = '.';
    dot[1].attr = ATTR_DIRECTORY;
    // Parent is root (cluster 0 for convenience in DOS, or root cluster)
    // Actually for root, it should be 0.
    dot[1].fst_clus_hi = 0;
    dot[1].fst_clus_lo = 0;

    uint32_t lba = fat32_cluster_to_lba(vol, cluster);
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return -5;

    memset(buffer, 0, vol->bytes_per_sector);
    memcpy(buffer, dot, sizeof(dot));

    vol->write_sector(lba, buffer);
    kfree(buffer);

    return 0;
}

void fat32_list_directory(fat32_volume_t* vol) {
    uint32_t current_cluster = vol->root_cluster;
    uint8_t* buffer = kmalloc(vol->bytes_per_sector);
    if (!buffer) return;

    hal_console_write("  [FAT32] Listing Root Directory:\n");

    while (current_cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, current_cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (vol->read_sector(lba + i, buffer) != 0) {
                hal_console_write("Error reading directory sector\n");
                kfree(buffer);
                return;
            }

            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)buffer;
            int entries_per_sector = vol->bytes_per_sector / sizeof(fat32_dir_entry_t);

            for (int j = 0; j < entries_per_sector; j++) {
                if (entry[j].name[0] == DIRENT_END) {
                    kfree(buffer);
                    return; // End of directory
                }
                if (entry[j].name[0] == DIRENT_DELETED) continue;
                if (entry[j].attr & ATTR_LONG_NAME) continue; // Skip LFN for now
                if (entry[j].attr & ATTR_VOLUME_ID) continue;

                char name_buf[13];
                fat32_get_name(entry[j].name, name_buf);

                hal_console_write("    - ");
                hal_console_write(name_buf);
                if (entry[j].attr & ATTR_DIRECTORY) {
                    hal_console_write("/");
                }
                hal_console_write("\n");
            }
        }
        current_cluster = fat32_get_next_cluster(vol, current_cluster);
    }
    kfree(buffer);
}

int fat32_find_file(fat32_volume_t* vol, const char* filename, fat32_dir_entry_t* out_entry) {
    return fat32_get_dirent_loc(vol, filename, NULL, NULL, out_entry);
}

int fat32_read_file(fat32_volume_t* vol, const char* filename, void* buffer, uint32_t size) {
    fat32_dir_entry_t entry;
    if (fat32_find_file(vol, filename, &entry) != 0) {
        return -1; // File not found
    }

    if (entry.attr & ATTR_DIRECTORY) return -2; // Cannot read directory as file

    uint32_t cluster = (uint32_t)entry.fst_clus_hi << 16 | entry.fst_clus_lo;
    uint32_t bytes_read = 0;
    uint32_t bytes_to_read = (size < entry.file_size) ? size : entry.file_size;
    uint8_t* out_ptr = (uint8_t*)buffer;

    uint8_t* sector_buffer = kmalloc(vol->bytes_per_sector);
    if (!sector_buffer) return -3;

    while (bytes_read < bytes_to_read && cluster < FAT32_EOC) {
        uint32_t lba = fat32_cluster_to_lba(vol, cluster);

        for (uint32_t i = 0; i < vol->sectors_per_cluster; i++) {
            if (bytes_read >= bytes_to_read) break;

            if (vol->read_sector(lba + i, sector_buffer) != 0) {
                kfree(sector_buffer);
                return -4; // Read error
            }

            uint32_t chunk = vol->bytes_per_sector;
            if (bytes_read + chunk > bytes_to_read) {
                chunk = bytes_to_read - bytes_read;
            }

            memcpy(out_ptr + bytes_read, sector_buffer, chunk);
            bytes_read += chunk;
        }
        cluster = fat32_get_next_cluster(vol, cluster);
    }

    kfree(sector_buffer);
    return bytes_read;
}
