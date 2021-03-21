#include <ctype.h>
#include <string.h>

#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <aos/lmp_chan.h>
#include <aos/aos_rpc.h>
#include <barrelfish_kpi/paging_arm_v8.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>
#include <spawn/argv.h>

extern struct bootinfo *bi;
extern coreid_t my_core_id;





/**
 * \brief Set the base address of the .got (Global Offset Table) section of the ELF binary
 *
 * \param arch_load_info This must be the base address of the .got section (local to the
 * child's VSpace). Must not be NULL.
 * \param handle The handle for the new dispatcher that is to be spawned. Must not be NULL.
 * \param enabled_area The "resume enabled" register set. Must not be NULL.
 * \param disabled_area The "resume disabled" register set. Must not be NULL.
 */
__attribute__((__used__))
static void armv8_set_registers(void *arch_load_info,
                              dispatcher_handle_t handle,
                              arch_registers_state_t *enabled_area,
                              arch_registers_state_t *disabled_area)
{
    assert(arch_load_info != NULL);
    uintptr_t got_base = (uintptr_t) arch_load_info;

    struct dispatcher_shared_aarch64 * disp_arm = get_dispatcher_shared_aarch64(handle);
    disp_arm->got_base = got_base;

    enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = got_base;
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = got_base;
}









/**
 * TODO(M2): Implement this function.
 * \brief Spawn a new dispatcher called 'argv[0]' with 'argc' arguments.
 *
 * This function spawns a new dispatcher running the ELF binary called
 * 'argv[0]' with 'argc' - 1 additional arguments. It fills out 'si'
 * and 'pid'.
 *
 * \param argc The number of command line arguments. Must be > 0.
 * \param argv An array storing 'argc' command line arguments.
 * \param si A pointer to the spawninfo struct representing
 * the child. It will be filled out by this function. Must not be NULL.
 * \param pid A pointer to a domainid_t variable that will be
 * assigned to by this function. Must not be NULL.
 * \return Either SYS_ERR_OK if no error occured or an error
 * indicating what went wrong otherwise.
 */
errval_t spawn_load_argv(int argc, char *argv[], struct spawninfo *si,
                domainid_t *pid) {
    // TODO: Implement me
    // - Initialize the spawn_info struct
    // - Get the module from the multiboot image
    //   and map it (take a look at multiboot.c)
    // - Setup the child's cspace
    // - Setup the child's vspace
    // - Load the ELF binary
    // - Setup the dispatcher
    // - Setup the environment
    // - Make the new dispatcher runnable
    errval_t err;
    char* name = argv[0];
    DEBUG_PRINTF("Spawning process: %s\n", name);
    /*err = spawn_load_by_name(name, si, pid);
    if (err_is_fail(err)){
        return err;
    }*/


    struct capref cnode_child_l1;
    struct cnoderef child_ref;
    err = cnode_create_l1(&cnode_child_l1, &child_ref);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_ROOTCN);
    }

    DEBUG_PRINTF("cnode_child_l1 slot is: %d\n", cnode_child_l1.slot);

    struct cnoderef taskcn;
    struct cnoderef basepagecn;
    struct cnoderef pagecn;
    
    struct cnoderef alloc0;
    struct cnoderef alloc1;
    struct cnoderef alloc2;
    
    err = cnode_create_foreign_l2(cnode_child_l1, ROOTCN_SLOT_TASKCN, &taskcn);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_TASKCN);
    }

    err = cnode_create_foreign_l2(cnode_child_l1, ROOTCN_SLOT_BASE_PAGE_CN, &basepagecn);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_PAGECN);
    }

    for (int i = 0; i < 256; i++) {
        struct capref counter = (struct capref) {
            .cnode = basepagecn,
            .slot = i
        };
        struct capref ram;
        err = ram_alloc(&ram, BASE_PAGE_SIZE);
        if (err_is_fail(err)) {
            HERE;
            return err_push(err, SPAWN_ERR_FILL_SMALLCN);
        }
        err = cap_copy(counter, ram);
        if (err_is_fail(err)) { HERE; return err; }
    }

    err = cnode_create_foreign_l2(cnode_child_l1, ROOTCN_SLOT_PAGECN, &pagecn);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_PAGECN);
    }

    err = cnode_create_foreign_l2(cnode_child_l1, ROOTCN_SLOT_SLOT_ALLOC0, &alloc0);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_SLOTALLOC_CNODE);
    }

    err = cnode_create_foreign_l2(cnode_child_l1, ROOTCN_SLOT_SLOT_ALLOC1, &alloc1);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_SLOTALLOC_CNODE);
    }

    err = cnode_create_foreign_l2(cnode_child_l1, ROOTCN_SLOT_SLOT_ALLOC2, &alloc2);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_SLOTALLOC_CNODE);
    }

    struct capref child_selfep = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_SELFEP
    };
    struct capref child_dispatcher = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_DISPATCHER
    };
    struct capref child_rootcn = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_ROOTCN
    };
    struct capref child_dispframe = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_DISPFRAME
    };
    struct capref child_argspage = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_ARGSPAGE
    };

    err = dispatcher_create(child_dispatcher);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_DISPATCHER);
    }

    err = cap_retype(child_selfep, child_dispatcher, 0, ObjType_EndPointLMP, 0, 1);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_SELFEP);
    }

    err = cap_copy(child_rootcn, cnode_child_l1);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_COPY_MODULECN);
    }
    
    err = frame_create(child_dispframe, DISPATCHER_FRAME_SIZE, NULL);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_DISPATCHER_FRAME);
    }


    // ===========================================
    // create l0 vnode and initialize paging state
    // ===========================================
    struct capref child_l0_vnodecap = (struct capref) {
        .cnode = pagecn,
        .slot = 0
    };
    struct capref l0_vnode;

    err = slot_alloc(&l0_vnode);
    if (err_is_fail(err)) {
        HERE;
        return err;
    }
    err = vnode_create(child_l0_vnodecap, ObjType_VNode_AARCH64_l0);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_VNODE);
    }
    err = cap_copy(l0_vnode, child_l0_vnodecap);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_COPY_VNODE);
    }

    paging_init_state_foreign(&si->ps, VADDR_OFFSET, l0_vnode, get_default_slot_allocator());

    struct capref argframe;
    err = slot_alloc(&argframe);
    if (err_is_fail(err)) { HERE; return err; }

    err = frame_create(child_argspage, BASE_PAGE_SIZE, NULL);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_CREATE_ARGSPG);
    }
    err = cap_copy(argframe, child_argspage);
    if (err_is_fail(err)) { HERE; return err; }

    // TODO: organize vspace
    void* arg_ptr;
    err = paging_map_frame_complete(get_current_paging_state(), &arg_ptr, argframe, NULL, NULL);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_ARGSPG_TO_SELF);
    }
    lvaddr_t child_arg_ptr = 0x500000UL * 0x1000;
    err = paging_map_fixed_attr(&si->ps, child_arg_ptr, argframe, BASE_PAGE_SIZE, VREGION_FLAGS_READ_WRITE);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_ARGSPG_TO_NEW);
    }

    memset(arg_ptr, 0, BASE_PAGE_SIZE);
    struct spawn_domain_params *sdp = arg_ptr;
    sdp->argc = argc;
    lvaddr_t child_argv_ptr = child_arg_ptr + sizeof(struct spawn_domain_params);
    char* argv_ptr = arg_ptr + sizeof(struct spawn_domain_params);
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (argv_ptr + len + 8 > ((char*)arg_ptr) + BASE_PAGE_SIZE) {
            return SPAWN_ERR_ARGSPG_OVERFLOW;
        }
        memcpy(argv_ptr, argv[i], len + 1);
        sdp->argv[i] = (const char*) child_argv_ptr;
        argv_ptr += len + 1;
        child_argv_ptr += len + 1;
    }

    genvaddr_t retentry;
    elf_load(EM_AARCH64, &allocate_elf_memory, &si->ps, si->mapped_elf, si->mapped_elf_size, &retentry);

    struct Elf64_Shdr* got = elf64_find_section_header_name(si->mapped_elf, si->mapped_elf_size, ".got");
    lvaddr_t got_base_address_in_childs_vspace = got->sh_addr;

    /* DEBUG_PRINTF("cnode_child_l2 slot is: %d", cnode_child_l2.slot); */

    struct capref dispframe;
    err = slot_alloc(&dispframe);
    if (err_is_fail(err)) { HERE; return err; }
    err = cap_copy(dispframe, child_dispframe);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_COPY_KERNEL_CAP);
    }

    uint64_t dispaddr = 0x612345 * DISPATCHER_FRAME_SIZE;

    err = paging_map_fixed_attr(&si->ps, dispaddr, dispframe, DISPATCHER_FRAME_SIZE, VREGION_FLAGS_READ_WRITE_NOCACHE);

    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_DISPATCHER_TO_NEW);
    }
    err = paging_map_fixed_attr(get_current_paging_state(), dispaddr, dispframe, DISPATCHER_FRAME_SIZE, VREGION_FLAGS_READ_WRITE_NOCACHE);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_DISPATCHER_TO_SELF);
    }
    dispatcher_handle_t handle = dispaddr;
    struct dispatcher_shared_generic *disp = get_dispatcher_shared_generic(handle);
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(handle);
    arch_registers_state_t *enabled_area = dispatcher_get_enabled_save_area(handle);
    arch_registers_state_t *disabled_area = dispatcher_get_disabled_save_area(handle);

    registers_set_param(enabled_area, child_arg_ptr);

    disp_gen->core_id = disp_get_core_id(); // core id of the process
    disp->udisp = dispaddr; // Virtual address of the dispatcher frame in child’s VSpace
    disp->disabled = 1;// Start in disabled mode
    strncpy(disp->name, "hello_world", DISP_NAME_LEN); // A name (for debugging)
    disabled_area->named.pc = retentry; // Set program counter (where it should start to execute)
    // Initialize offset registers
    // got_addr is the address of the .got in the child’s VSpace
    armv8_set_registers((void*) got_base_address_in_childs_vspace, handle, enabled_area, disabled_area);
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;


    err = slot_alloc(&si->dispatcher);
    if (err_is_fail(err)) { HERE; return err; }

    err = cap_copy(si->dispatcher, child_dispatcher);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_COPY_KERNEL_CAP);
    }

    err = invoke_dispatcher(si->dispatcher, cap_dispatcher, cnode_child_l1, child_l0_vnodecap, child_dispframe, true);
    if (err_is_fail(err)) {
        HERE;
        return err;
    }
    
    dump_dispatcher(disp);

    /*err = invoke_dispatcher(si->dispatcher, NULL_CAP, NULL_CAP, NULL_CAP, NULL_CAP, true);
    if (err_is_fail(err)) {
        HERE;
        return err;
    }
    dump_dispatcher(disp);*/
    return SYS_ERR_OK;
}


errval_t allocate_elf_memory(void* state, genvaddr_t base, size_t size, uint32_t flags, void **ret)
{
    struct paging_state *st = (struct paging_state*) state;

    errval_t err = SYS_ERR_OK;
    struct capref frame;
    size_t actual_size;
    err = frame_alloc(&frame, size, &actual_size);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_MODULE);
    }

    err = paging_map_fixed_attr(st, base, frame, actual_size, flags);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_MODULE);
    }

    err = paging_map_frame(get_current_paging_state(), ret, actual_size, frame, NULL, NULL);
    if (err_is_fail(err)) {
        HERE;
        return err_push(err, SPAWN_ERR_MAP_MODULE);
    }

    return err;
}

/**
 * TODO(M2): Implement this function.
 * \brief Spawn a new dispatcher executing 'binary_name'
 *
 * \param binary_name The name of the binary.
 * \param si A pointer to a spawninfo struct that will be
 * filled out by spawn_load_by_name. Must not be NULL.
 * \param pid A pointer to a domainid_t that will be
 * filled out by spawn_load_by_name. Must not be NULL.
 *
 * \return Either SYS_ERR_OK if no error occured or an error
 * indicating what went wrong otherwise.
 */
errval_t spawn_load_by_name(char *binary_name, struct spawninfo * si,
                            domainid_t *pid) {
    // - Get the mem_region from the multiboot image
    // - Fill in argc/argv from the multiboot command line
    // - Call spawn_load_argv
    errval_t err = SYS_ERR_OK;

    //TODO: is  bi correctly initialized by the init/usr/main.c
    struct mem_region* mem_region = multiboot_find_module(bi, binary_name);

    //this mem_region should be of type module
    assert(mem_region->mr_type == RegionType_Module);
    struct capability cap;
    struct capref child_frame = {
        .cnode = cnode_module,
        .slot = mem_region->mrmod_slot
    };

    err = invoke_cap_identify(child_frame, &cap);
    if (err_is_fail(err)) {
        return err;
    }

    size_t mapping_size = get_size(&cap);
    char* elf_address;
    paging_map_frame_attr(get_current_paging_state(), (void **) &elf_address,
                          mapping_size, child_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL);

    si->mapped_elf = (lvaddr_t) elf_address;
    si->mapped_elf_size = (size_t) mapping_size;
    debug_printf("ELF address = %lx\n", elf_address);
    debug_printf("%x, '%c', '%c', '%c'\n", elf_address[0], elf_address[1], elf_address[2], elf_address[3]);
    debug_printf("BOI\n");

    char *argv[1] = { binary_name };
    return spawn_load_argv(1, argv, si, pid);
}
