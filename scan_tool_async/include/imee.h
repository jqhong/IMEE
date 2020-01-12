struct arg_blk
{
    uint64_t cr3;
    uint64_t num_x_page;
    uint64_t num_w_page;
    uint64_t offset;
    uint64_t code_host;
    uint64_t data_host;
    uint64_t stack;
    uint64_t entry;
    uint64_t int_handler;
    uint64_t thunk;
    uint64_t got;
    uint64_t got_len;
    uint64_t gotplt;
    uint64_t gotplt_len;
};

int setup_imee (void* args, void* p1);
int run_imee ();
