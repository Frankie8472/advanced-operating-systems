#include <ctype.h>
#include <string.h>

#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <aos/lmp_chan.h>
#include <aos/aos_rpc.h>
#include <barrelfish_kpi/paging_arm_v8.h>
#include <target/aarch64/aos/dispatcher_target.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>
#include <spawn/argv.h>
#include <spawn/process_manager.h>
#include <string.h>

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

    struct dispatcher_shared_aarch64 *disp_arm = get_dispatcher_shared_aarch64(handle);
    disp_arm->got_base = got_base;

    enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = got_base;
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = got_base;
}

/**
 * \brief Setup the C-Space for a new dispatcher
 * \param cnode_child_l1 L1 CNode of the new C-Space.
 * \param taskcn References to Level 2 CNodes to initialize.
 * \param basepagecn
 * \param pagecn
 * \param alloc0
 * \param alloc1
 * \param alloc2
 * \return Either SYS_ERR_OK if no error occured or an error
 * indicating what went wrong otherwise.
 */
errval_t setup_c_space(struct capref cnode_l1,
                       struct cnoderef *taskcn,
                       struct cnoderef *basepagecn,
                       struct cnoderef *pagecn,
                       struct cnoderef *alloc0,
                       struct cnoderef *alloc1,
                       struct cnoderef *alloc2)
{
    errval_t err = cnode_create_foreign_l2(cnode_l1, ROOTCN_SLOT_TASKCN, taskcn);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_TASKCN);

    err = cnode_create_foreign_l2(cnode_l1, ROOTCN_SLOT_BASE_PAGE_CN, basepagecn);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_PAGECN);

    for (int i = 0; i < 256; i++) {
        struct capref counter = (struct capref) {
            .cnode = *basepagecn,
            .slot = i
        };
        struct capref ram;
        err = ram_alloc(&ram, BASE_PAGE_SIZE);
        ON_ERR_PUSH_RETURN(err, SPAWN_ERR_FILL_SMALLCN);

        err = cap_copy(counter, ram);
        ON_ERR_RETURN(err);
    }

    err = cnode_create_foreign_l2(cnode_l1, ROOTCN_SLOT_PAGECN, pagecn);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_PAGECN);

    err = cnode_create_foreign_l2(cnode_l1, ROOTCN_SLOT_SLOT_ALLOC0, alloc0);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_SLOTALLOC_CNODE);

    err = cnode_create_foreign_l2(cnode_l1, ROOTCN_SLOT_SLOT_ALLOC1, alloc1);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_SLOTALLOC_CNODE);

    err = cnode_create_foreign_l2(cnode_l1, ROOTCN_SLOT_SLOT_ALLOC2, alloc2);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_SLOTALLOC_CNODE);

    return SYS_ERR_OK;
}


errval_t spawn_setup_dispatcher(int argc, const char *const argv[], struct spawninfo *si,
                domainid_t *pid)
{
    errval_t err;
    const char* name = argv[0];
    DEBUG_PRINTF("Spawning process: %s\n", name);

    struct capref cnode_child_l1;
    struct cnoderef child_ref;
    err = cnode_create_l1(&cnode_child_l1, &child_ref);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_ROOTCN);

    si->rootcn = cnode_child_l1;

    //DEBUG_PRINTF("cnode_child_l1 slot is: %d\n", cnode_child_l1.slot);

    struct cnoderef taskcn;
    struct cnoderef basepagecn;
    struct cnoderef pagecn;

    struct cnoderef alloc0;
    struct cnoderef alloc1;
    struct cnoderef alloc2;

    err = setup_c_space(cnode_child_l1, &taskcn, &basepagecn, &pagecn, &alloc0, &alloc1, &alloc2);

    // endpoint to itself in child cspace
    struct capref child_ep_cap = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_SELFEP
    };
    struct capref child_stdout_cap = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_STDOUT_EP
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
    struct capref init_ep_cap = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_INITEP
    };
    struct capref mm_ep_cap = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_MEMORYEP
    };
    struct capref spawner_ep_cap = (struct capref) {
        .cnode = taskcn,
        .slot = TASKCN_SLOT_SPAWNER_EP
    };

    err = dispatcher_create(child_dispatcher);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_DISPATCHER);

    err = cap_retype(child_ep_cap, child_dispatcher, 0, ObjType_EndPointLMP, 0, 1);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_SELFEP);

    err = cap_copy(child_rootcn, cnode_child_l1);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_COPY_MODULECN);

    err = frame_create(child_dispframe, DISPATCHER_FRAME_SIZE, NULL);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_DISPATCHER_FRAME);

    // setup the channel from the init side

    err = endpoint_create(LMP_RECV_LENGTH, &si->cap_ep, &si->lmp_ep);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);

    err = cap_copy(init_ep_cap, si->cap_ep);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY_FAIL);

    err = endpoint_create(LMP_RECV_LENGTH, &si->child_stdout_cap, &si->child_stdout);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);

    err = cap_copy(child_stdout_cap, si->child_stdout_cap);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY_FAIL);

    err = cap_copy(mm_ep_cap, cap_mmep);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY_FAIL);

/* <<<<<<< HEAD */
    /*     struct capref irq = (struct capref) { */
    /*         .cnode = taskcn, */
    /*         .slot = TASKCN_SLOT_IRQ */
    /*     }; */
    /*     cap_copy(irq, cap_irq); */
    /* } else if (strncmp(si->binary_name, "enet", 32) == 0) { */
    /*     // TODO */
    /*     debug_printf("special enet stuff\n"); */
    /*     size_t source_addr = get_phys_addr(dev_frame); */
    /*     err = cap_retype(child_dev_frame, dev_frame, IMX8X_ENET_BASE - source_addr, ObjType_DevFrame, IMX8X_ENET_SIZE, 1); */
    /*     DEBUG_ERR(err, "oh no\n"); */
    /*     ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE); */

    /*     struct capref irq = (struct capref) { */
    /*         .cnode = taskcn, */
    /*         .slot = TASKCN_SLOT_IRQ */
    /*     }; */
    /*     cap_copy(irq, cap_irq); */
    /*     debug_printf("DONE\n"); */
/* ======= */
    if (!capref_is_null(si->spawner_ep_cap)) {
        err = cap_copy(spawner_ep_cap, si->spawner_ep_cap);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY_FAIL);
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
    ON_ERR_RETURN(err);

    err = vnode_create(child_l0_vnodecap, ObjType_VNode_AARCH64_l0);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_VNODE);

    err = cap_copy(l0_vnode, child_l0_vnodecap);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_COPY_VNODE);

    paging_init_state_foreign(&si->ps, VADDR_OFFSET, l0_vnode, get_default_slot_allocator());

    struct capref argframe;
    err = frame_alloc(&argframe, BASE_PAGE_SIZE, NULL);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_CREATE_ARGSPG);

    err = cap_copy(child_argspage, argframe);
    ON_ERR_RETURN(err);

    void* arg_ptr;
    err = paging_map_frame_complete(get_current_paging_state(), &arg_ptr, argframe, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_ARGSPG_TO_SELF);


    // TODO: organize vspace
    lvaddr_t child_arg_ptr = 0x700000UL * 0x1000;
    err = paging_map_fixed_attr(&si->ps, child_arg_ptr, argframe, BASE_PAGE_SIZE, VREGION_FLAGS_READ_WRITE);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_ARGSPG_TO_NEW);

    memset(arg_ptr, 0, BASE_PAGE_SIZE);
    struct spawn_domain_params *sdp = arg_ptr;
    sdp->argc = argc;
    lvaddr_t child_argv_ptr = child_arg_ptr + sizeof(struct spawn_domain_params);
    char* argv_ptr = arg_ptr + sizeof(struct spawn_domain_params);
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (argv_ptr + len + 1 > ((char*) arg_ptr) + BASE_PAGE_SIZE) {
            return err_push(err, SPAWN_ERR_ARGSPG_OVERFLOW);
        }
        memcpy(argv_ptr, argv[i], len + 1);
        sdp->argv[i] = (const char*) child_argv_ptr;
        argv_ptr += len + 1;
        child_argv_ptr += len + 1;
    }

    genvaddr_t retentry;
    err = elf_load(EM_AARCH64, &allocate_elf_memory, &si->ps, si->mapped_elf, si->mapped_elf_size, &retentry);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_LOAD);

    struct Elf64_Shdr *got = elf64_find_section_header_name(si->mapped_elf, si->mapped_elf_size, ".got");
    NULLPTR_CHECK(got, SPAWN_ERR_LOAD);

    //debug_printf("0x%lx -> 0x%lx\n", si->mapped_elf, si->mapped_elf_size);
    lvaddr_t got_base_address_in_childs_vspace = got->sh_addr;
    //debug_printf("possible 0x%lx\n", got_base_address_in_childs_vspace);
    //lvaddr_t got_base_offset = got->sh_addr - si->mapped_elf;

    struct capref dispframe;
    err = slot_alloc(&dispframe);
    ON_ERR_RETURN(err);

    err = cap_copy(dispframe, child_dispframe);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_COPY_KERNEL_CAP);

    uint64_t dispaddr = ROUND_UP(0x12345678, DISPATCHER_FRAME_SIZE);

    err = paging_map_fixed_attr(&si->ps, dispaddr, dispframe, DISPATCHER_FRAME_SIZE, VREGION_FLAGS_READ_WRITE);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_DISPATCHER_TO_NEW);

    void* dispaddr_init;
    err = paging_map_frame(get_current_paging_state(), &dispaddr_init, DISPATCHER_FRAME_SIZE, dispframe, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_DISPATCHER_TO_SELF);

    memset(dispaddr_init, 0, DISPATCHER_FRAME_SIZE);

    dispatcher_handle_t handle = (dispatcher_handle_t) dispaddr_init;
    struct dispatcher_shared_generic *disp = get_dispatcher_shared_generic(handle);
    //struct dispatcher_aarch64 *disp_aarch64 = (struct dispatcher_aarch64*)handle;
    struct dispatcher_generic *disp_gen = get_dispatcher_generic(handle);
    arch_registers_state_t *enabled_area = dispatcher_get_enabled_save_area(handle);
    arch_registers_state_t *disabled_area = dispatcher_get_disabled_save_area(handle);
    // disp_gen->domain_id = *pid = spawn_get_new_domainid();

    registers_set_param(enabled_area, child_arg_ptr);

    disp_gen->core_id = disp_get_core_id(); // core id of the process
    //disp_aarch64->generic.current;
    disp->udisp = dispaddr; // Virtual address of the dispatcher frame in child’s VSpace
    disp->disabled = 1;// Start in disabled mode
    strncpy(disp->name, si->binary_name, DISP_NAME_LEN); // A name (for debugging)
    disabled_area->named.pc = retentry; // Set program counter (where it should start to execute)

    // Initialize offset registers
    armv8_set_registers((void*) got_base_address_in_childs_vspace, handle, enabled_area, disabled_area);
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;

    err = slot_alloc(&si->dispatcher);
    ON_ERR_RETURN(err);



    err = cap_copy(si->dispatcher, child_dispatcher);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_COPY_KERNEL_CAP);

    aos_rpc_init_lmp(&si->rpc, cap_selfep, NULL_CAP, si->lmp_ep, NULL);


    err = register_process_to_process_manager((char*)name, pid);
    ON_ERR_RETURN(err);
    disp_gen->domain_id = *pid;

    if(get_mem_online()){
        disp_gen -> core_state.c.mem_online = true;
    }
    if(get_pm_online()){
        disp_gen -> core_state.c.pm_online = true;
    }

    si->dispatcher_cap = cap_dispatcher;
    si->dispframe_cap = child_dispframe;

    //dump_dispatcher(disp);
    return SYS_ERR_OK;
}


errval_t spawn_invoke_dispatcher(struct spawninfo *si)
{
    struct cnoderef child_pagecn = {
        .croot = get_cap_addr(si->rootcn),
        .cnode = ROOTCN_SLOT_ADDR(ROOTCN_SLOT_PAGECN),
        .level = CNODE_TYPE_OTHER
    };
    struct capref vnode_l0 = (struct capref) {
        .cnode = child_pagecn,
        .slot = 0
    };

    return invoke_dispatcher(
        si->dispatcher,
        si->dispatcher_cap,
        si->rootcn,
        vnode_l0,
        si->dispframe_cap,
        true
    );
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
errval_t spawn_load_argv(int argc, const char *const argv[], struct spawninfo *si,
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

    err = spawn_setup_dispatcher(argc, argv, si, pid);
    ON_ERR_RETURN(err);

    debug_printf("%s set up!\n", si->binary_name);

    err = spawn_invoke_dispatcher(si);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}




errval_t allocate_elf_memory(void* state, genvaddr_t base, size_t size, uint32_t flags, void **ret)
{
    struct paging_state *st = (struct paging_state*) state;

    genvaddr_t real_base = ROUND_DOWN(base, BASE_PAGE_SIZE);
    genvaddr_t real_size = ROUND_UP(size + (base - real_base), BASE_PAGE_SIZE);

    int64_t offset_in_page = base - real_base;


    int actual_flags = 0;
    if (flags & PF_R)
        actual_flags |= VREGION_FLAGS_READ;
    if (flags & PF_W)
        actual_flags |= VREGION_FLAGS_WRITE;
    if (flags & PF_X)
        actual_flags |= VREGION_FLAGS_EXECUTE;

    //debug_printf("ALLOC ELF STUFF: 0x%lx -> 0x%lx\n", base, base + size);

    errval_t err = SYS_ERR_OK;
    struct capref frame;
    size_t actual_size;
    err = frame_alloc(&frame, real_size, &actual_size);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_MODULE);

    //debug_printf("MAPPING ELF STUFF: 0x%lx -> 0x%lx\n", real_base, real_base + actual_size);
    err = paging_map_fixed_attr(st, real_base, frame, actual_size, actual_flags);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_MODULE);

    err = paging_map_frame(get_current_paging_state(), ret, actual_size, frame, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, SPAWN_ERR_MAP_MODULE);

    *ret += offset_in_page;

    return SYS_ERR_OK;
}

static int get_argc(char * args){
   int argc = 1;
   int i = 0;
   while(args[i] != '\0'){
     if(args[i] == ' ' && args[i + 1] != '\0'){argc++;}

     i++;
   }
   // if(argc >= 1){argc++;}
   return argc;
 }
static void create_argv(char* args,char *argv[]){
  int i = 0;
  int j = 1;
  argv[0] = &args[0];
  while(args[i] != '\0'){
    if(args[i] == ' '){
      args[i] = '\0';
      argv[j] = &args[i + 1];
      j++;
    }
    i++;
  }
}

static void strip_extra_spaces(char* str) {
  int i, x;
  for(i=x=0; str[i]; ++i)
    if(!isspace(str[i]) || (i > 0 && !isspace(str[i-1])))
      str[x++] = str[i];
  str[x] = '\0';
}

errval_t spawn_load_by_name_setup(char *binary_name, struct spawninfo *si)
{
    errval_t err = SYS_ERR_OK;

    //TODO: is bi correctly initialized by the init/usr/main.c
    struct mem_region* mem_region = multiboot_find_module(bi, binary_name);

    if (mem_region == NULL) {
        return SPAWN_ERR_MAP_MODULE;
    }
    
    //this mem_region should be of type module
    assert(mem_region->mr_type == RegionType_Module);
    struct capability cap;
    struct capref child_frame = {
        .cnode = cnode_module,
        .slot = mem_region->mrmod_slot
    };

    ON_ERR_RETURN(err);

    size_t mapping_size = get_size(&cap);
    char* elf_address;
    
    err = paging_map_frame_attr(get_current_paging_state(), (void **) &elf_address,
                          mapping_size, child_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL);
    ON_ERR_RETURN(err);

    si->mapped_elf = (lvaddr_t) elf_address;
    si->mapped_elf_size = (size_t) mem_region->mrmod_size;

    char *args_string = (char *)  multiboot_module_opts(mem_region);
    char copy[strlen(args_string)];
    strcpy(copy,args_string);
    strip_extra_spaces(copy);

    int argc = get_argc(copy);
    char  const *argv[argc];
    create_argv(copy, (char **) argv);

    // set binary name to full name
    si->binary_name = binary_name;

    return SYS_ERR_OK;
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
errval_t spawn_load_by_name(char *binary_name, struct spawninfo *si,
                            domainid_t *pid) {
    // - Get the mem_region from the multiboot image
    // - Fill in argc/argv from the multiboot command line
    // - Call spawn_load_argv
    errval_t err = SYS_ERR_OK;
    // debug_printf("Halli Hallo\n");

    //TODO: is  bi correctly initialized by the init/usr/main.c
    struct mem_region* mem_region = multiboot_find_module(bi, binary_name);
    
    if (mem_region == NULL) {
        return SPAWN_ERR_MAP_MODULE;
    }


    
    //this mem_region should be of type module
    assert(mem_region->mr_type == RegionType_Module);
    struct capability cap;
    struct capref child_frame = {
        .cnode = cnode_module,
        .slot = mem_region->mrmod_slot
    };
    err = invoke_cap_identify(child_frame, &cap);
    char bbb[128];
    debug_print_cap_at_capref(bbb, 128, child_frame);
    // debug_printf("Hello, %s\n", bbb);

    ON_ERR_RETURN(err);

    size_t mapping_size = get_size(&cap);
    // debug_printf("capsize: %ld\n", mapping_size);
    char* elf_address;
    
    paging_map_frame_attr(get_current_paging_state(), (void **) &elf_address,
                          mapping_size, child_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL);

    // debug_printf("Hello\n");
    si->mapped_elf = (lvaddr_t) elf_address;
    si->mapped_elf_size = (size_t) mem_region->mrmod_size;
    // debug_printf("ELF address = %lx\n", elf_address);
    // debug_printf("%x, '%c', '%c', '%c'\n", elf_address[0], elf_address[1], elf_address[2], elf_address[3]);
    // debug_printf("BOI\n");

    char *args_string = (char *)  multiboot_module_opts(mem_region);
    char copy[strlen(args_string)];
    strcpy(copy,args_string);
    strip_extra_spaces(copy);

    int argc = get_argc(copy);
    char  const *argv[argc];
    create_argv(copy, (char **) argv);

    // set binary name to full name
    si->binary_name = binary_name;

    return spawn_load_argv(argc, argv, si, pid);
}


errval_t register_process_to_process_manager(char* binary_name,domainid_t* pid){

    if(!get_pm_online()){
        debug_printf("Pm is not online!\n");
        return SYS_ERR_OK;
    }

    errval_t err;
    coreid_t core_id = disp_get_core_id();
    if(core_id == 0){
        struct aos_rpc* pm_rpc =  get_pm_rpc();
        debug_printf("calling register process %s\n", binary_name);
        err = aos_rpc_call(pm_rpc,AOS_RPC_REGISTER_PROCESS,core_id,binary_name,pid);
        ON_ERR_RETURN(err);
    }else{
        assert(get_core_channel(0) && "UMP channel to core 0 is not present!");
        err = aos_rpc_call(get_core_channel(0),AOS_RPC_REGISTER_PROCESS,core_id,binary_name,pid);
    }
    return SYS_ERR_OK;
}
