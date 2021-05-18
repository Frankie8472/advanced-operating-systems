errval_t handle_ARP(struct enet_queue* q, struct devq_buf* buf,
                    lvaddr_t p_addr, struct enet_driver_state* st);
errval_t handle_packet(struct enet_queue* q, struct devq_buf* buf,
                       struct enet_driver_state* st);
