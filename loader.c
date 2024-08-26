#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "loader.h"

Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;

/*
 * release memory and other cleanups
 */
void loader_cleanup() {
    if (ehdr) free(ehdr);
    if (fd >= 0) close(fd);
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char *exe) {
    // 1. Open the ELF file
    fd = open(exe, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open ELF file");
        exit(1);
    }

    // 2. Load the ELF header
    ehdr = (Elf32_Ehdr*)malloc(sizeof(Elf32_Ehdr));
    if (read(fd, ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
        perror("Failed to read ELF header");
        loader_cleanup();
        exit(1);
    }

    // 3. Check if it's a valid ELF file
    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        printf("Invalid ELF file\n");
        loader_cleanup();
        exit(1);
    }

    // 4. Load the Program Header Table
    phdr = (Elf32_Phdr*)malloc(ehdr->e_phnum * sizeof(Elf32_Phdr));
    lseek(fd, ehdr->e_phoff, SEEK_SET);
    if (read(fd, phdr, ehdr->e_phnum * sizeof(Elf32_Phdr)) != ehdr->e_phnum * sizeof(Elf32_Phdr)) {
        perror("Failed to read Program Header Table");
        loader_cleanup();
        exit(1);
    }

    // 5. Iterate over the program headers to find the PT_LOAD segment
    Elf32_Addr entry_point = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // Allocate memory for the segment
            void *mem = mmap((void*)phdr[i].p_vaddr, phdr[i].p_memsz,
                             PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mem == MAP_FAILED) {
                perror("Failed to allocate memory with mmap");
                loader_cleanup();
                exit(1);
            }

            // Load the segment into memory
            lseek(fd, phdr[i].p_offset, SEEK_SET);
            if (read(fd, mem, phdr[i].p_filesz) != phdr[i].p_filesz) {
                perror("Failed to load segment into memory");
                loader_cleanup();
                exit(1);
            }

            // Check if this segment contains the entry point
            if (ehdr->e_entry >= phdr[i].p_vaddr && ehdr->e_entry < phdr[i].p_vaddr + phdr[i].p_memsz) {
                entry_point = ehdr->e_entry;
            }
        }
    }

    if (!entry_point) {
        printf("Failed to find entry point in loaded segments\n");
        loader_cleanup();
        exit(1);
    }

    // 6. Jump to the entry point
    int (*entry)() = (int (*)())entry_point;
    int result = entry();
    printf("User _start return value = %d\n", result);

    // 7. Cleanup
    loader_cleanup();
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        printf("Usage: %s <ELF Executable> \n",argv[0]);
        exit(1);
    }
    // 1. Carry out necessary checks on the input ELF file
    // 2. Passing it to the loader for carrying out the loading/execution
    load_and_run_elf(argv[1]);
    // 3. Invoke the cleanup routine inside the loader
    return 0;
}
