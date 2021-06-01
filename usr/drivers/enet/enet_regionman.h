struct dev_list {
    struct devq_buf* cur;
    struct dev_list* next;
};

// struct to keep track of an entire enet-region
struct enet_qstate {
    struct enet_queue* queue;
    struct dev_list* free;
};

errval_t init_enet_qstate(struct enet_queue* queue,
                                struct enet_qstate* tgt);

void qstate_append_free(struct enet_qstate* qs,
                               struct dev_list* fr);

errval_t dequeue_bufs(struct enet_qstate* qs);

errval_t get_free_buf(struct enet_qstate* qs, struct devq_buf* ret);

errval_t enqueue_buf(struct enet_qstate* qs, struct devq_buf* buf);
