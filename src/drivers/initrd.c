#include "initrd.h"
#include "heap.h"
#include "utils.h"
#include "uart_console.h"

// The raw initrd header
typedef struct {
    uint32_t nfiles;
} initrd_header_t;

initrd_header_t* initrd_header;
initrd_file_header_t* file_headers;
fs_node_t* initrd_root;
fs_node_t* root_nodes; // Array of file nodes
int n_root_nodes;

// Read operation for a specific file
uint32_t initrd_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    initrd_file_header_t header = file_headers[node->inode];
    
    if (offset > header.length)
        return 0;
        
    if (offset + size > header.length)
        size = header.length - offset;
        
    // Calculate physical address of data
    // Data starts after nfiles + headers
    // location + header.offset
    // But header.offset is relative to start of data area or start of file?
    // Let's assume absolute offset from initrd start for simplicity.
    
    uint8_t* data = (uint8_t*)((uint32_t)initrd_header + header.offset);
    
    memcpy(buffer, data + offset, size);
    
    return size;
}

// Directory listing (Find file by name)
fs_node_t* initrd_finddir(fs_node_t* node, char* name) {
    (void)node; // Root only has flat files
    
    for (int i = 0; i < n_root_nodes; i++) {
        if (strcmp(name, root_nodes[i].name) == 0) {
            return &root_nodes[i];
        }
    }
    return NULL;
}

fs_node_t* initrd_init(uint32_t location) {
    uart_print("[INITRD] Initializing at address: ");
    // TODO: Print hex location
    uart_print("\n");
    
    initrd_header = (initrd_header_t*)location;
    file_headers = (initrd_file_header_t*)(location + sizeof(initrd_header_t));
    
    // Check sanity (assuming nfiles < 100)
    if (initrd_header->nfiles > 100) {
        uart_print("[INITRD] Warning: Suspicious file count. Corrupt image?\n");
        return NULL;
    }

    n_root_nodes = initrd_header->nfiles;
    
    // Create Root Directory Node
    initrd_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    strcpy(initrd_root->name, "initrd");
    initrd_root->flags = FS_DIRECTORY;
    initrd_root->finddir = &initrd_finddir;
    initrd_root->inode = 0;
    
    // Create File Nodes
    root_nodes = (fs_node_t*)kmalloc(sizeof(fs_node_t) * n_root_nodes);
    
    for (int i = 0; i < n_root_nodes; i++) {
        file_headers[i].offset += location; // Relocate offset to absolute address
        
        strcpy(root_nodes[i].name, file_headers[i].name);
        root_nodes[i].length = file_headers[i].length;
        root_nodes[i].inode = i;
        root_nodes[i].flags = FS_FILE;
        root_nodes[i].read = &initrd_read;
        
        // Debug
        uart_print("  Found file: ");
        uart_print(root_nodes[i].name);
        uart_print("\n");
    }
    
    return initrd_root;
}
