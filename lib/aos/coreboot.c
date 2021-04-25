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
 * \brief Boot a core
 *
 * \param mpid          The ARM MPID of the core to be booted    
 * \param boot_driver   Name of the boot driver binary
 * \param cpu_driver    Name of the CPU driver
 * \param init          The name of the init binary
 * \param urpc_frame_id Description of what will be passed as URPC frame
 *
 */


extern uint64_t *aaadata;
uint64_t *aaadata = 0;

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

    debug_printf("Starting coreboot\n");
    errval_t err;
    struct capref KCB;
    struct capref KCB_Ram;
    struct capref stack_cap;
    struct capref core_data_cap;
    struct capref init_space;
    size_t ret_size;

    //TODO: check alignment of these frame allocations?
    // err = frame_alloc(&KCB,OBJSIZE_KCB,&ret_size); 
    err = ram_alloc(&KCB_Ram,OBJSIZE_KCB);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to allocated frame for KCB in coreboot\n");

    }
    err = slot_alloc(&KCB);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Slot alloc failed in coreboot\n");
    }
    err = cap_retype(KCB,KCB_Ram,0,ObjType_KernelControlBlock,OBJSIZE_KCB,1);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to retype ram cap into KCB cap in coreboot\n");
    }



    //err = frame_alloc_and_map(&stack_cap,16*BASE_PAGE_SIZE,&ret_size,&stack_pointer);
    err = ram_alloc(&stack_cap, 16 * BASE_PAGE_SIZE);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to allocate frame for new kernel stack in coreboot");
    }

    //assert(ret_size >= 16 * BASE_PAGE_SIZE && "Returned frame less than 16 Pages for kernel stack");


    struct armv8_core_data* core_data;
    err = frame_alloc_and_map(&core_data_cap,BASE_PAGE_SIZE,&ret_size,(void ** ) &core_data);
    if(err_is_fail(err)){
       DEBUG_ERR(err,"Failed to allocate and core_data in coreboot\n");
    }
     assert(ret_size >= BASE_PAGE_SIZE && "Returned frame is not large enough to hold core data structure context in coreboot");
    

    err = frame_alloc(&init_space,ARMV8_CORE_DATA_PAGES * BASE_PAGE_SIZE,&ret_size);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to alloc space for init process\n");
    }
    assert(ret_size >= BASE_PAGE_SIZE * ARMV8_CORE_DATA_PAGES && "Size for init process is not large enough\n");




    struct paging_state *st = get_current_paging_state();



    // Load boot driver
    //====================================================
    struct mem_region* boot_driver_mem_region = multiboot_find_module(bi, boot_driver);

    assert(boot_driver_mem_region -> mr_type == RegionType_Module);
    struct capref boot_driver_cap = {
        .cnode = cnode_module,
        .slot = boot_driver_mem_region->mrmod_slot
    };
    void* old_boot_binary;  
    err = paging_map_frame_complete_readable(get_current_paging_state(), (void **) &old_boot_binary,boot_driver_cap,NULL,NULL);
    debug_printf("ELF address = %lx\n", old_boot_binary);
    char* obb = (char*)old_boot_binary;
    debug_printf("%x, '%c', '%c', '%c'\n", obb[0], obb[1], obb[2], obb[3]);
    debug_printf("BOI\n");

    uintptr_t sindex;
    struct Elf64_Sym *elf_sym = elf64_find_symbol_by_name((genvaddr_t) old_boot_binary, boot_driver_mem_region->mr_bytes, "boot_entry_psci", 0, STT_FUNC, &sindex);
    debug_printf("Here is the boot entry address: %lx\n", elf_sym->st_value);


    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to map elf module for boot driver in coreboot\n");
    }
    struct capref new_boot_driver_cap;
    void* new_boot_binary;
    size_t boot_size = boot_driver_mem_region->mrmod_size;    
    err = frame_alloc(&new_boot_driver_cap, boot_size,&ret_size);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to allocate frame for boot driver binary in coreboot\n");
    }
    assert(ret_size >= boot_size && "Frame for bootdriver is too small in coreboot");
    err = paging_map_frame_attr(get_current_paging_state(), (void **) &new_boot_binary,
                    ret_size, new_boot_driver_cap,VREGION_FLAGS_READ_WRITE,NULL,NULL);


    struct capability boot_capability;
    invoke_cap_identify(new_boot_driver_cap, &boot_capability);
    struct mem_info mi = {
        .buf = new_boot_binary,
        .size = get_size(&boot_capability),
        .phys_base = get_address(&boot_capability),
    };
    
    genvaddr_t boot_driver_entry;

    load_elf_binary((genvaddr_t) old_boot_binary, &mi, elf_sym->st_value, &boot_driver_entry);
    debug_printf("boot driver phys mem: 0x%lx\n", get_address(&boot_capability));
    debug_printf("boot driver reloc entry: 0x%lx\n", boot_driver_entry);
    debug_printf("boot load offsett: 0x%lx\n", 0);
    //relocate_elf((genvaddr_t) old_boot_binary, &mi, get_address(&boot_capability));
    
    debug_printf("reloc_entry_point: 0x%lx\n", boot_driver_entry);
    

    // Load cpu driver
    //====================================================
    struct mem_region* cpu_driver_mem_region = multiboot_find_module(bi,cpu_driver);
    assert(cpu_driver_mem_region -> mr_type == RegionType_Module);
    struct capref cpu_driver_mod_cap = {
        .cnode = cnode_module,
        .slot = cpu_driver_mem_region->mrmod_slot
    };
    void* cpu_driver_mr = NULL;

    paging_map_frame_complete(st, &cpu_driver_mr, cpu_driver_mod_cap, NULL, NULL);
    
    struct capref cpu_driver_rc;
    err = frame_alloc(&cpu_driver_rc, cpu_driver_mem_region->mrmod_size, &ret_size);
    
    void *cpu_driver_mem_new;

    err = paging_map_frame_attr(st, (void **) &cpu_driver_mem_new,
                    ret_size, cpu_driver_rc, VREGION_FLAGS_READ_WRITE,NULL,NULL);
    
    struct capability cpu_driver_ram;
    invoke_cap_identify(cpu_driver_rc, &cpu_driver_ram);
    mi = (struct mem_info) {
        .buf = cpu_driver_mem_new,
        .size = get_size(&cpu_driver_ram),
        .phys_base = get_address(&cpu_driver_ram),
    };
    sindex = 0;
    struct Elf64_Sym *cpu_driver_start = elf64_find_symbol_by_name((genvaddr_t) cpu_driver_mr, cpu_driver_mem_region->mr_bytes, "arch_init", 0, STT_FUNC, &sindex);
    //debug_printf("cpu_driver_start: %lx\n", cpu_driver_start->st_value);
    if (cpu_driver_start == NULL) {
        debug_printf("error: could not find arch_init\n");
    }

    // genvaddr_t cpu;
    genvaddr_t cpu_entry;
    debug_printf("cpu_driver_mem: 0x%lx\n", cpu_driver_mr);
    load_elf_binary((genvaddr_t) cpu_driver_mr, &mi, cpu_driver_start->st_value, &cpu_entry);
    
    debug_printf("cpu driver entry after load: 0x%lx\n", &cpu_entry);
    debug_printf("cpu load offset: 0x%lx\n", ARMv8_KERNEL_OFFSET);
    cpu_entry += ARMv8_KERNEL_OFFSET;
    relocate_elf((genvaddr_t) cpu_driver_mr, &mi, ARMv8_KERNEL_OFFSET);
    debug_printf("cpu driver entry after relocate: 0x%lx\n", cpu_entry);
    //debug_printf("cpu_driver_start_reloc: 0x%lx\n", cpu_driver_start_reloc);







    debug_printf("Addr of boot_driver: %lx\n",boot_driver_mem_region -> mr_base);
    debug_printf("Size of boot_driver: %lx\n",boot_driver_mem_region -> mrmod_size);
    debug_printf("Ptrdiff boot_driver: %lx\n",boot_driver_mem_region -> mrmod_data);
    debug_printf("addr of cpu region: %lx\n",cpu_driver_mem_region -> mr_base);
    debug_printf("size of cpu region: %lx\n",cpu_driver_mem_region -> mrmod_size);
    debug_printf("Ptrdiff cpu driver: %lx\n",boot_driver_mem_region -> mrmod_data);

    
    //genvaddr_t reloc_entry_point;
    //err = load_elf_binary((genvaddr_t) old_boot_binary,&boot_mem_info,elf_sym -> st_value,&reloc_entry_point);
    //
    
    struct mem_region* init_region = multiboot_find_module(bi, init);
    struct capref init_region_cap = {
        .cnode = cnode_module,
        .slot = init_region->mrmod_slot
    };
    genpaddr_t init_region_phys = get_phys_addr(init_region_cap);
    
    core_data->monitor_binary = (struct armv8_coredata_memreg) {
        .base = init_region_phys,
        .length = init_region->mrmod_size
    };


    //Write core_data struct
    core_data -> boot_magic = ARMV8_BOOTMAGIC_PSCI;
    core_data -> cpu_driver_stack = get_phys_addr(stack_cap) + get_phys_size(stack_cap);
    core_data -> cpu_driver_stack_limit = get_phys_addr(stack_cap);
    // core_data -> cpu_driver_entry = //virtual address of cpu driver entry
    memset(core_data->cpu_driver_cmdline, 0, sizeof core_data->cpu_driver_cmdline);
    core_data -> memory.base  = get_phys_addr(init_space);
    core_data -> memory.length = get_phys_size(init_space);
    core_data -> kcb = get_phys_addr(KCB_Ram);
    
    
    core_data -> src_core_id = disp_get_core_id();
    core_data -> dst_core_id = mpid;
    core_data -> src_arch_id = disp_get_core_id();
    core_data -> dst_arch_id = mpid;

    genpaddr_t context = get_phys_addr(core_data_cap);

    uint64_t psci_use_hvc = 0; //This is ignored by i.MX8, doesnt matter


    cpu_dcache_wbinv_range((vm_offset_t)core_data, get_phys_size(core_data_cap));
    cpu_dcache_wbinv_range((vm_offset_t)new_boot_binary, boot_driver_mem_region->mrmod_size);
    cpu_dcache_wbinv_range((vm_offset_t)cpu_driver_mem_new, cpu_driver_mem_region->mrmod_size);
    //cpu_dcache_wbinv_range((vm_offset_t) stack_pointer,get_phys_size(stack_cap));
    cpu_nullop();

    size_t len = 1024 * 1024 * 4;
    uint64_t *arr = malloc(len);
    for (int i = 0; i < len; i++) {
        arr[i] = i % 12345;
        if (i > 123) {
            arr[i % 123] += arr[i] % 3;
            arr[i] += arr[i % 123] % 5;
        }
    }
    aaadata = arr;



    err = invoke_monitor_spawn_core(mpid, CPU_ARM8, boot_driver_entry, context, psci_use_hvc);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to invoke core in coreboot c");
    }

    return SYS_ERR_OK;

}