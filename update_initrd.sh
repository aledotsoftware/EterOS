cat << 'INNER_EOF' > /tmp/initrd_patch.diff
--- kernel/fs/initrd.c
+++ kernel/fs/initrd.c
@@ -21,6 +21,14 @@
     struct initrd_dir *next;
 } initrd_dir_t;

+typedef struct initrd_vfile {
+    char name[128];
+    uint32_t inode;
+    uint8_t *data;
+    uint32_t size;
+    struct initrd_vfile *next;
+} initrd_vfile_t;
+
 static fs_node_t *initrd_root = NULL;
 static uint8_t *initrd_start = NULL;
 static uint32_t initrd_image_size = 0;
@@ -29,6 +37,9 @@
 static initrd_dir_t *virtual_dirs = NULL;
 static uint32_t virtual_dirs_count = 0;

+static initrd_vfile_t *virtual_files = NULL;
+static uint32_t virtual_files_count = 0;
+
 static uint8_t initrd_extract_child(const char* full_path, const char* parent_prefix, char* child_out, size_t max_len) {
     if (!full_path) return 0;
     size_t offset = 0;
@@ -52,10 +63,22 @@
     return NULL;
 }

+static initrd_vfile_t *find_virtual_file(const char *path) {
+    initrd_vfile_t* current = virtual_files;
+    while (current) {
+        if (strcmp(current->name, path) == 0) return current;
+        current = current->next;
+    }
+    return NULL;
+}
+
 ssize_t initrd_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
     if (!node || !buffer) return 0;
-    if (node->inode >= file_count) return 0;

+    initrd_vfile_t *vf = NULL;
+    if (node->inode >= 0xE0000000 && node->inode < 0xF0000000) {
+        for (vf = virtual_files; vf; vf = vf->next) { if (vf->inode == node->inode) break; }
+    } else if (node->inode >= file_count) return 0;
+
+    uint32_t file_size = vf ? vf->size : file_headers[node->inode].size;
+
-    uint32_t file_size = file_headers[node->inode].size;
     if (offset >= file_size) return 0;
     if (offset + size > file_size) size = file_size - offset;

+    if (vf) {
+        memcpy(buffer, vf->data + offset, size);
+    } else {
         memcpy(buffer, initrd_start + file_headers[node->inode].offset + offset, size);
+    }
     return size;
 }

+ssize_t initrd_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
+    if (!node || !buffer) return -1;
+
+    initrd_vfile_t *vf = NULL;
+    if (node->inode >= 0xE0000000 && node->inode < 0xF0000000) {
+        for (vf = virtual_files; vf; vf = vf->next) { if (vf->inode == node->inode) break; }
+    }
+
+    if (!vf) return -1; // Cannot write to static files yet unless we convert them
+
+    if (offset + size > vf->size) {
+        uint8_t *new_data = kmalloc(offset + size);
+        if (!new_data) return -1;
+        if (vf->data) {
+            memcpy(new_data, vf->data, vf->size);
+            kfree(vf->data);
+        }
+        vf->data = new_data;
+        vf->size = offset + size;
+        node->length = vf->size;
+    }
+    memcpy(vf->data + offset, buffer, size);
+    return size;
+}
+
+int initrd_create(fs_node_t *parent, char *name, uint16_t permission) {
+    (void)permission;
+    char target[256];
+    const char* prefix = initrd_dir_prefix(parent);
+
+    if (prefix && prefix[0] != '\0') {
+        strlcpy(target, prefix, sizeof(target));
+        strlcat(target, "/", sizeof(target));
+        strlcat(target, name, sizeof(target));
+    } else {
+        strlcpy(target, name, sizeof(target));
+    }
+
+    if (find_virtual_file(target) != NULL) return -1;
+
+    initrd_vfile_t *new_vf = (initrd_vfile_t*)kmalloc(sizeof(initrd_vfile_t));
+    if (!new_vf) return -2;
+    strlcpy(new_vf->name, target, sizeof(new_vf->name));
+    new_vf->inode = 0xE0000000 + virtual_files_count++;
+    new_vf->data = NULL;
+    new_vf->size = 0;
+    new_vf->next = virtual_files;
+    virtual_files = new_vf;
+
+    return 0;
+}
+
 fs_node_t *initrd_make_dir_node(const char* display_name, const char* full_path) {
     fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
     if (!node) return NULL;
@@ -99,6 +143,15 @@

     uint32_t seen = 0;

+    for (initrd_vfile_t *vf = virtual_files; vf; vf = vf->next) {
+        char child[128];
+        if (initrd_extract_child(vf->name, prefix, child, sizeof(child))) {
+            if (seen == index) {
+                strlcpy(entry->name, child, sizeof(entry->name));
+                entry->inode = vf->inode;
+                return 0;
+            }
+            seen++;
+        }
+    }
+
     for (initrd_dir_t* cur = virtual_dirs; cur; cur = cur->next) {
         char child[128];
         if (initrd_extract_child(cur->name, prefix, child, sizeof(child))) {
@@ -282,6 +335,27 @@
         return fnode;
     }

+    initrd_vfile_t *vf = find_virtual_file(target);
+    if (vf) {
+        fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
+        if (!fnode) return 0;
+        memset(fnode, 0, sizeof(fs_node_t));
+        fnode->ref_count = 1;
+        size_t nlen = strlen(name);
+        if (nlen >= sizeof(fnode->name)) nlen = sizeof(fnode->name) - 1;
+        memcpy(fnode->name, name, nlen);
+        fnode->name[nlen] = '\0';
+        fnode->inode = vf->inode;
+        fnode->mask = 0644;
+        fnode->flags = FS_FILE;
+        fnode->read = &initrd_read;
+        fnode->write = &initrd_write;
+        fnode->length = vf->size;
+        return fnode;
+    }
+
     for (uint32_t i = 0; i < file_count; i++) {
         size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
         if (name_len > 64) name_len = 64; /* Cap name length for security */
@@ -308,6 +382,21 @@
              fnode->flags = FS_FILE;
              fnode->read = &initrd_read;
-             fnode->write = 0;
+             fnode->write = &initrd_write;
              fnode->open = 0;
              fnode->close = 0;
              fnode->readdir = 0;
@@ -401,6 +490,7 @@
     initrd_root->readdir = &initrd_readdir;
     initrd_root->finddir = &initrd_finddir;
     initrd_root->mkdir = &initrd_mkdir;
+    initrd_root->create = &initrd_create;
     initrd_root->ptr = 0;
     initrd_root->impl = 0;

INNER_EOF
patch -p0 < /tmp/initrd_patch.diff
