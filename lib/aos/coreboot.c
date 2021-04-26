#include <aos/aos.h>
#include <aos/coreboot.h>
#include <spawn/multiboot.h>
#include <elf/elf.h>
#include <string.h>
#include <barrelfish_kpi/arm_core_data.h>
#include <aos/kernel_cap_invocations.h>
#include <aos/cache.h>



#define ARMv8_KERNEL_OFFSET 0xffff000000000000

extern struct bootinfo *bi;

struct mem_info {
    size_t                size;      // Size in bytes of the memory region
    void                  *buf;      // Address where the region is currently mapped // -- virtual address?

    lpaddr_t              phys_base; // Physical base address   
};

/**
 * Load a ELF image into memory.
 *
 * binary:            Valid pointer to ELF image in current address space
 * mem:               Where the ELF will be loaded
 * entry_point:       Virtual address of the entry point
 * reloc_entry_point: Return the loaded, physical address of the entry_point
 */
__attribute__((__used__))
static errval_t load_elf_binary(genvaddr_t binary, const struct mem_info *mem,
                         genvaddr_t entry_point, genvaddr_t *reloc_entry_point)

{

    struct Elf64_Ehdr *ehdr = (struct Elf64_Ehdr *)binary;

    /* Load the CPU driver from its ELF image. */
    bool found_entry_point= 0;
    bool loaded = 0;

    struct Elf64_Phdr *phdr = (struct Elf64_Phdr *)(binary + ehdr->e_phoff);
    for(size_t i= 0; i < ehdr->e_phnum; i++) {
        if(phdr[i].p_type != PT_LOAD) {
            DEBUG_PRINTF("Segment %d load address 0x% "PRIx64 ", file size %" PRIu64
                  ", memory size 0x%" PRIx64 " SKIP\n", i, phdr[i].p_vaddr,
                  phdr[i].p_filesz, phdr[i].p_memsz);
            continue;
        }

        DEBUG_PRINTF("Segment %d load address 0x% "PRIx64 ", file size %" PRIu64
              ", memory size 0x%" PRIx64 " LOAD\n", i, phdr[i].p_vaddr,
              phdr[i].p_filesz, phdr[i].p_memsz);


        if (loaded) {
            USER_PANIC("Expected one load able segment!\n");
        }
        loaded = 1;

        void *dest = mem->buf;
        lpaddr_t dest_phys = mem->phys_base;

        assert(phdr[i].p_offset + phdr[i].p_memsz <= mem->size);

        /* copy loadable part */
        memcpy(dest, (void *)(binary + phdr[i].p_offset), phdr[i].p_filesz);

        /* zero out BSS section */
        memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);

        if (!found_entry_point) {
            if(entry_point >= phdr[i].p_vaddr
                 && entry_point - phdr[i].p_vaddr < phdr[i].p_memsz) {
               *reloc_entry_point= (dest_phys + (entry_point - phdr[i].p_vaddr));
               found_entry_point= 1;
            }
        }
    }

    if (!found_entry_point) {
        USER_PANIC("No entry point loaded\n");
    }

    return SYS_ERR_OK;
}



/**
 * Relocate an already loaded ELF image. 
 *
 * binary:            Valid pointer to ELF image in current address space
 * mem:               Where the ELF is loaded
 * kernel_:       Virtual address of the entry point
 * reloc_entry_point: Return the loaded, physical address of the entry_point
 */
__attribute__((__used__))
static errval_t
relocate_elf(genvaddr_t binary, struct mem_info *mem, lvaddr_t load_offset)
{
    DEBUG_PRINTF("Relocating image.\n");

    struct Elf64_Ehdr *ehdr = (struct Elf64_Ehdr *)binary;

    size_t shnum  = ehdr->e_shnum;
    struct Elf64_Phdr *phdr = (struct Elf64_Phdr *)(binary + ehdr->e_phoff);
    struct Elf64_Shdr *shead = (struct Elf64_Shdr *)(binary + (uintptr_t)ehdr->e_shoff);

    /* Search for relocaton sections. */
    for(size_t i= 0; i < shnum; i++) {

        struct Elf64_Shdr *shdr=  &shead[i];
        if(shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) {
            if(shdr->sh_info != 0) {
                DEBUG_PRINTF("I expected global relocations, but got"
                              " section-specific ones.\n");
                return ELF_ERR_HEADER;
            }


            uint64_t segment_elf_base= phdr[0].p_vaddr;
            uint64_t segment_load_base=mem->phys_base;
            uint64_t segment_delta= segment_load_base - segment_elf_base;
            uint64_t segment_vdelta= (uintptr_t)mem->buf - segment_elf_base;

            size_t rsize;
            if(shdr->sh_type == SHT_REL){
                rsize= sizeof(struct Elf64_Rel);
            } else {
                rsize= sizeof(struct Elf64_Rela);
            }

            assert(rsize == shdr->sh_entsize);
            size_t nrel= shdr->sh_size / rsize;

            void * reldata = (void*)(binary + shdr->sh_offset);

            /* Iterate through the relocations. */
            for(size_t ii= 0; ii < nrel; ii++) {
                void *reladdr= reldata + ii *rsize;

                switch(shdr->sh_type) {
                    case SHT_REL:
                        DEBUG_PRINTF("SHT_REL unimplemented.\n");
                        return ELF_ERR_PROGHDR;
                    case SHT_RELA:
                    {
                        struct Elf64_Rela *rel= reladdr;

                        uint64_t offset= rel->r_offset;
                        uint64_t sym= ELF64_R_SYM(rel->r_info);
                        uint64_t type= ELF64_R_TYPE(rel->r_info);
                        uint64_t addend= rel->r_addend;

                        uint64_t *rel_target= (void *)offset + segment_vdelta;

                        switch(type) {
                            case R_AARCH64_RELATIVE:
                                if(sym != 0) {
                                    DEBUG_PRINTF("Relocation references a"
                                                 " dynamic symbol, which is"
                                                 " unsupported.\n");
                                    return ELF_ERR_PROGHDR;
                                }

                                /* Delta(S) + A */
                                *rel_target= addend + segment_delta + load_offset;
                                //debug_printf("Here is the relocation address: 0x%lx\n",*rel_target);
                                break;

                            default:
                                DEBUG_PRINTF("Unsupported relocation type %d\n",
                                             type);
                                return ELF_ERR_PROGHDR;
                        }
                    }
                    break;
                    default:
                        DEBUG_PRINTF("Unexpected type\n");
                        break;

                }
            }
        }
    }

    return SYS_ERR_OK;
}


/**
 * \brief maps a module into our vspace
 */
static errval_t map_module(struct mem_region *module, void **ret_addr)
{
    assert(module->mr_type == RegionType_Module);

    struct capref module_cap = {
        .cnode = cnode_module,
        .slot = module->mrmod_slot
    };

    return paging_map_frame_complete_readable(get_current_paging_state(), ret_addr, module_cap, NULL, NULL);
}

/**
 * \brief allocated memory to load, then loads and relocates an elf image
 * 
 * \param mi will be filled in with the allocated memory
 */
static errval_t load_and_relocate(void *elf_image, size_t elf_image_size, struct mem_info *mi,
                                  const char *entry_sym_name, size_t relocate_offset,
                                  genpaddr_t *entry)
{
    errval_t err;
    uintptr_t eindex = 0;
    struct Elf64_Sym *entry_sym = elf64_find_symbol_by_name((genvaddr_t) elf_image, elf_image_size, entry_sym_name, 0, STT_FUNC, &eindex);
    if (entry_sym == NULL) {
        return SPAWN_ERR_ELF_FIND_SYMBOL;
    }
    genvaddr_t boot_entry = entry_sym->st_value;
    
    size_t size = elf_virtual_size((lvaddr_t) elf_image);
    
    struct capref frame;
    err = frame_alloc_and_map(&frame, size, &mi->size, &mi->buf);
    mi->phys_base = get_phys_addr(frame);
    
    err = load_elf_binary((genvaddr_t) elf_image, mi, boot_entry, &boot_entry);
    ON_ERR_RETURN(err);
    err = relocate_elf((genvaddr_t) elf_image, mi, relocate_offset);
    ON_ERR_RETURN(err);
    
    *entry = boot_entry + relocate_offset;

    return SYS_ERR_OK;
}



/**
 * \brief Boot a core
 *
 * \param mpid          The ARM MPID of the core to be booted    
 * \param boot_driver   Name of the boot driver binary
 * \param cpu_driver    Name of the CPU driver
 * \param init          The name of the init binary
 * \param urpc_frame_id Description of what will be passed as URPC frame
 *
 */
errval_t coreboot(coreid_t mpid,
        const char *boot_driver,
        const char *cpu_driver,
        const char *init,
        struct frame_identity urpc_frame_id)
{
    // Implement me!
    // - Get a new KCB by retyping a RAM cap to ObjType_KernelControlBlock.
    //   Note that it should at least OBJSIZE_KCB, and it should also be aligned 
    //   to a multiple of 16k.
    // - Get and load the CPU and boot driver binary.
    // - Relocate the boot and CPU driver. The boot driver runs with a 1:1
    //   VA->PA mapping. The CPU driver is expected to be loaded at the 
    //   high virtual address space, at offset ARMV8_KERNEL_OFFSET.
    // - Allocate a page for the core data struct
    // - Allocate stack memory for the new cpu driver (at least 16 pages)
    // - Fill in the core data struct, for a description, see the definition
    //   in include/target/aarch64/barrelfish_kpi/arm_core_data.h
    // - Find the CPU driver entry point. Look for the symbol "arch_init". Put
    //   the address in the core data struct. 
    // - Find the boot driver entry point. Look for the symbol "boot_entry_psci"
    // - Flush the cache.
    // - Call the invoke_monitor_spawn_core with the entry point 
    //   of the boot driver and pass the (physical, of course) address of the 
    //   boot struct as argument.
    
    errval_t err;
    struct paging_state *st = get_current_paging_state();
    
    assert(st);

    // ==========================
    // Alloc Kernel Control Block
    // ==========================
    struct capref kcb_ram;
    struct capref kcb;
    err = ram_alloc(&kcb_ram, OBJSIZE_KCB);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RAM_ALLOC);
    err = slot_alloc(&kcb);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);
    err = cap_retype(kcb, kcb_ram, 0, ObjType_KernelControlBlock, OBJSIZE_KCB, 1);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);


    // ==========================================
    // Load and relocate Boot Driver & CPU Driver
    // ==========================================
    struct mem_region *boot_mod = multiboot_find_module(bi, boot_driver);
    struct mem_region *cpu_driver_mod = multiboot_find_module(bi, cpu_driver);
    NULLPTR_CHECK(boot_mod, SPAWN_ERR_FIND_MODULE);
    NULLPTR_CHECK(cpu_driver_mod, SPAWN_ERR_FIND_MODULE);

    void *boot_elf_blob;
    void *cpu_driver_elf_blob;

    struct mem_info boot_mi;
    struct mem_info cpu_driver_mi;
    genpaddr_t boot_entry;
    genpaddr_t cpu_driver_entry;

    err = map_module(boot_mod, &boot_elf_blob);
    ON_ERR_RETURN(err);
    err = load_and_relocate(boot_elf_blob, boot_mod->mrmod_size, &boot_mi, "boot_entry_psci", 0, &boot_entry);
    ON_ERR_RETURN(err);

    err = map_module(cpu_driver_mod, &cpu_driver_elf_blob);
    ON_ERR_RETURN(err);
    err = load_and_relocate(cpu_driver_elf_blob, cpu_driver_mod->mrmod_size, &cpu_driver_mi, "arch_init", ARMv8_KERNEL_OFFSET, &cpu_driver_entry);
    ON_ERR_RETURN(err);


    // ==============================
    // Alloc Stack for new CPU Driver
    // ==============================
    struct capref stack_ram;
    err = ram_alloc(&stack_ram, 16 * BASE_PAGE_SIZE);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RAM_ALLOC);


    // ================
    // Locate init blob
    // ================
    struct mem_region *init_mod = multiboot_find_module(bi, init);
    NULLPTR_CHECK(init_mod, SPAWN_ERR_FIND_MODULE);
    struct capref init_mod_cap = {
        .cnode = cnode_module,
        .slot = init_mod->mrmod_slot
    };

    void *init_image;
    err = map_module(init_mod, &init_image);
    ON_ERR_RETURN(err);
    size_t init_size = elf_virtual_size((lvaddr_t) init_image);


    // ============
    // Alloc Memory 
    // ============
    struct capref init_ram;
    size_t init_ram_size;
    err = frame_alloc(&init_ram, ROUND_UP(init_size, BASE_PAGE_SIZE) + ARMV8_CORE_DATA_PAGES * BASE_PAGE_SIZE, &init_ram_size);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RAM_ALLOC);


    // =======================
    // Create Core Data struct
    // =======================
    struct capref core_data_frame;
    err = frame_alloc(&core_data_frame, BASE_PAGE_SIZE, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC);

    struct armv8_core_data *core_data;
    err = paging_map_frame_complete(st, (void **) &core_data, core_data_frame, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_DO_MAP);

    core_data->boot_magic = ARMV8_BOOTMAGIC_PSCI;
    core_data->cpu_driver_stack_limit = get_phys_addr(stack_ram);
    core_data->cpu_driver_stack = core_data->cpu_driver_stack_limit + get_phys_size(stack_ram);
    core_data->kcb = get_phys_addr(kcb_ram);
    core_data->cpu_driver_entry = cpu_driver_entry;

    core_data->src_core_id = disp_get_core_id();
    core_data->dst_core_id = mpid;
    core_data->src_arch_id = disp_get_core_id();
    core_data->dst_arch_id = mpid;

    core_data->monitor_binary = (struct armv8_coredata_memreg) {
        .base = get_phys_addr(init_mod_cap),
        .length = init_mod->mrmod_size
    };

    core_data->memory = (struct armv8_coredata_memreg) {
        .base = get_phys_addr(init_ram),
        .length = init_ram_size
    };


    core_data->urpc_frame = (struct armv8_coredata_memreg) {
        .base = urpc_frame_id.base,
        .length = urpc_frame_id.bytes
    };


    cpu_dcache_wbinv_range((vm_offset_t) boot_mi.buf, boot_mi.size);
    cpu_dcache_wbinv_range((vm_offset_t) cpu_driver_mi.buf, cpu_driver_mi.size);
    cpu_dcache_wbinv_range((vm_offset_t) core_data, sizeof *core_data);

    // ==========
    // Start core
    // ==========
    err = invoke_monitor_spawn_core(mpid, CPU_ARM8, boot_entry, get_phys_addr(core_data_frame), 0);
    
    return SYS_ERR_OK;
}

