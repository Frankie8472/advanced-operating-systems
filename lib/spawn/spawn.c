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
    DEBUG_PRINTF("Spawning process: %s",name);
    err = spawn_load_by_name(name,si,pid);
    if(err_is_fail(err)){
      return err;
    }

    struct capref cnode_child_l1;
    struct cnoderef child_ref;
    err = cnode_create_l1(&cnode_child_l1, &child_ref);
    if (err_is_fail(err)) {
      /* HERE; */
      return err_push(err, SPAWN_ERR_CREATE_ROOTCN);
    }

    DEBUG_PRINTF("cnode_child_l1 slot is: %d\n", cnode_child_l1.slot);

    /* struct cnoderef cnode_child_l2; */
    /* err = cnode_create_foreign_l2(cnode_child_l1, cnode_child_l1.slot, &cnode_child_l2); */
    HERE;
    /* DEBUG_PRINTF("cnode_child_l2 slot is: %d", cnode_child_l2.slot); */


    return LIB_ERR_NOT_IMPLEMENTED;
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
    struct mem_region* mem_region = multiboot_find_module(bi,binary_name);
    //this mem_region should be of type module
    assert(mem_region -> mr_type == RegionType_Module);
    struct capability cap;
    struct capref child_frame = {
      .cnode = cnode_module,
      .slot = mem_region -> mrmod_slot
    };
    err = invoke_cap_identify(child_frame,&cap);
    if(err_is_fail(err)){
      return err;
    }
    char* elf_address;

    paging_map_frame_attr(get_current_paging_state(), (void **) &elf_address,
                          4096, child_frame, VREGION_FLAGS_READ_WRITE, NULL, NULL);

    debug_printf("ELF address = %lx\n", elf_address);
    debug_printf("%x ", elf_address[0]);
    for(int i = 1; i < 4; ++i){
      debug_printf("%c ", elf_address[i]);
    }
    debug_printf("BOI\n");
    return err;
}
