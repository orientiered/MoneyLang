#ifndef ELF_WRITER_H
#define ELF_WRITER_H

#include <stdint.h>
#include <stdlib.h>
#include <elf.h>

const size_t ELF_SEFMENT_ALIGN = 0x1000;

Elf64_Ehdr generateElfHeader(uint64_t entryAddr, size_t pheaderCount);

Elf64_Phdr generateElfPheader(uint16_t permission, uint64_t offset, uint64_t vaddr, uint64_t size);


#endif
