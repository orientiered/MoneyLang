#include <stdlib.h>
#include <stdio.h>

#include "elfWriter.h"


Elf64_Ehdr generateElfHeader(uint64_t entryAddr, size_t pheaderCount) {

    Elf64_Ehdr header = {
        .e_ident = {
            [EI_MAG0]    = ELFMAG0,
            [EI_MAG1]    = ELFMAG1,
            [EI_MAG2]    = ELFMAG2,
            [EI_MAG3]    = ELFMAG3,
            [EI_CLASS]   = ELFCLASS64,
            [EI_DATA]    = ELFDATA2LSB,
            [EI_VERSION] = EV_CURRENT,
            [EI_OSABI]   = ELFOSABI_LINUX
        },
        .e_type    = ET_EXEC,
        .e_machine = EM_X86_64,
        .e_version = EV_CURRENT,
        .e_entry   = entryAddr,
        .e_phoff   = sizeof(Elf64_Ehdr),
        .e_ehsize  = sizeof(Elf64_Ehdr),
        .e_phentsize = sizeof(Elf64_Phdr),
        .e_phnum   = pheaderCount,
        // no section headers and string tables
        .e_shentsize = 0,
        .e_shnum = 0,
        .e_shstrndx = 0
    };


    return header;
}

Elf64_Phdr generateElfPheader(uint16_t permission, uint64_t offset, uint64_t vaddr, uint64_t size) {
    Elf64_Phdr progHeader = {
        .p_type   = PT_LOAD,
        .p_flags  = permission,
        .p_offset = offset,
        .p_vaddr  = vaddr,
        .p_paddr  = vaddr, // the same as vaddr
        .p_filesz = size,
        .p_memsz  = size,  // Assuming no uninitialized data
        .p_align  = ELF_SEFMENT_ALIGN // standard page alignment
    };

    return progHeader;
}
