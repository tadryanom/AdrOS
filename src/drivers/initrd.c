#include "initrd.h"
#include "utils.h"
#include "heap.h"
#include "uart_console.h"

initrd_file_header_t* file_headers;
fs_node_t* initrd_root;
fs_node_t* root_nodes; // Array of file nodes
int n_root_nodes;

uint32_t initrd_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);

uint32_t initrd_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    return initrd_read_impl(node, offset, size, buffer);
}

static uint32_t initrd_location_base = 0;

uint32_t initrd_read_impl(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    initrd_file_header_t header = file_headers[node->inode];
    
    if (offset > header.length) return 0;
    if (offset + size > header.length)
        size = header.length - offset;
        
    // Address = Base + Header_Offset + Read_Offset
    uint8_t* src = (uint8_t*)(initrd_location_base + header.offset + offset);
    
    memcpy(buffer, src, size);
    return size;
}

struct fs_node* initrd_finddir(struct fs_node* node, char* name) {
    (void)node;
    for (int i = 0; i < n_root_nodes; i++) {
        if (strcmp(name, root_nodes[i].name) == 0)
            return &root_nodes[i];
    }
    return 0;
}

fs_node_t* initrd_init(uint32_t location) {
    initrd_location_base = location;
    
    // Read number of files (first 4 bytes)
    uint32_t* n_files_ptr = (uint32_t*)location;
    n_root_nodes = *n_files_ptr;
    
    uart_print("[INITRD] Found ");
    char buf[16]; itoa(n_root_nodes, buf, 10);
    uart_print(buf);
    uart_print(" files.\n");
    
    // Headers start right after n_files
    file_headers = (initrd_file_header_t*)(location + sizeof(uint32_t));
    
    // Allocate nodes for files
    root_nodes = (fs_node_t*)kmalloc(sizeof(fs_node_t) * n_root_nodes);
    
    for (int i = 0; i < n_root_nodes; i++) {
        file_headers[i].offset += location; // Fixup offset to be absolute address
        
        strcpy(root_nodes[i].name, file_headers[i].name);
        root_nodes[i].inode = i;
        root_nodes[i].length = file_headers[i].length;
        root_nodes[i].flags = FS_FILE;
        root_nodes[i].read = &initrd_read_impl;
        root_nodes[i].write = 0;
        root_nodes[i].open = 0;
        root_nodes[i].close = 0;
        root_nodes[i].finddir = 0;
    }
    
    // Create Root Directory Node
    initrd_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    strcpy(initrd_root->name, "initrd");
    initrd_root->flags = FS_DIRECTORY;
    initrd_root->finddir = &initrd_finddir;
    
    return initrd_root;
}
