#include "elf.h"

#include "../io/terminal.h"
#include "../std/string.h"
#include "../mem/paging/paging.h"
#include "../mem/alloc/page_frame_alloc.h"

bool elf_validate(const void* data) {
    if (data == NULL) return false;

    const elf64_ehdr_t* hdr = (const elf64_ehdr_t*)data;

    if (hdr->e_ident[EI_MAG0] != ELFMAG0 ||
        hdr->e_ident[EI_MAG1] != ELFMAG1 ||
        hdr->e_ident[EI_MAG2] != ELFMAG2 ||
        hdr->e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }

    if (hdr->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }

    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        return false;
    }

    if (hdr->e_machine != EM_X86_64) {
        return false;
    }

    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
        return false;
    }

    return true;
}

elf64_ehdr_t* elf_get_header(const void* data) {
    return (elf64_ehdr_t*)data;
}

elf64_phdr_t* elf_get_program_header(const void* data, int index) {
    elf64_ehdr_t* ehdr = elf_get_header(data);
    if (index >= ehdr->e_phnum) return NULL;

    uint64_t offset = ehdr->e_phoff + (index * ehdr->e_phentsize);
    return (elf64_phdr_t*)((uint8_t*)data + offset);
}

int elf_load(const void* elf_data, size_t size, uint64_t* out_entry) {
    if (!elf_validate(elf_data)) {
        printkf_error("elf_load(): invalid ELF file\n");
        return -1;
    }

    elf64_ehdr_t* ehdr = elf_get_header(elf_data);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        elf64_phdr_t* phdr = elf_get_program_header(elf_data, i);
        if (phdr == NULL) continue;

        if (phdr->p_type != PT_LOAD) continue;

        uint64_t vaddr = phdr->p_vaddr;
        uint64_t memsz = phdr->p_memsz;
        uint64_t filesz = phdr->p_filesz;
        uint64_t offset = phdr->p_offset;
        uint64_t vaddr_aligned = vaddr & ~0xFFF;
        uint64_t vaddr_offset = vaddr - vaddr_aligned;
        uint64_t total_size = vaddr_offset + memsz;
        uint64_t pages_needed = (total_size + 0xFFF) / 0x1000;

        for (uint64_t p = 0; p < pages_needed; p++) {
            void* phys_page = pfallocator_request_page();
            if (phys_page == NULL) {
                printkf_error("elf_load(): out of memory\n");
                return -1;
            }

            uint64_t hhdm_offset = page_get_offset();
            void* phys_addr = (void*)((uint64_t)phys_page - hhdm_offset);

            void* virt_addr = (void*)(vaddr_aligned + p * 0x1000);
            page_map_memory(virt_addr, phys_addr);

            memset(phys_page, 0, 0x1000);
        }

        if (filesz > 0) {
            const uint8_t* src = (const uint8_t*)elf_data + offset;
            uint8_t* dst = (uint8_t*)vaddr;
            memcpy(dst, src, filesz);
        }

        if (memsz > filesz) {
            uint8_t* bss_start = (uint8_t*)(vaddr + filesz);
            memset(bss_start, 0, memsz - filesz);
        }
    }

    *out_entry = ehdr->e_entry;

    return 0;
}
