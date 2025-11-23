#include "elf.h"
#include "fat16.h"
#include "terminal.h"
#include "string.h"

#define EXEC_BASE 0x01000000   // ELF loading physical address

int elf_load(const char *filename) {
    Elf32_Ehdr hdr;

    // Read ELF header
    if (fat16_read_partial(filename, &hdr, sizeof(hdr), 0) != sizeof(hdr)) {
        terminal_write_line("ELF: failed to read header");
        return 0;
    }

    // Check magic
    if (hdr.e_ident[0] != 0x7F || hdr.e_ident[1] != 'E' ||
        hdr.e_ident[2] != 'L' || hdr.e_ident[3] != 'F') {
        terminal_write_line("ELF: invalid format");
        return 0;
    }

    // Load program headers
    for (int i = 0; i < hdr.e_phnum; i++) {

        Elf32_Phdr ph;
        uint32_t phOffset = hdr.e_phoff + i * hdr.e_phentsize;

        fat16_read_partial(filename, &ph, sizeof(ph), phOffset);

        if (ph.p_type != PT_LOAD) continue;

        uint8_t *dest = (uint8_t*)(ph.p_vaddr);

        // load segment
        fat16_read_partial(filename, dest, ph.p_filesz, ph.p_offset);

        // zero BSS
        k_memset(dest + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
    }

    // Jump to entry point
    void (*entry)() = (void(*)()) (hdr.e_entry);

    terminal_write("ELF executing at entry 0x");
    terminal_write_hex((uint32_t)entry);
    terminal_putc('\n');

    entry();

    return 1;
}