
#include "afl-qemu-cpu.h"

uint8_t *fuzz = NULL;
bool input_mode_SHM = false;
long fuzz_size = 0;
long fuzz_cursor = 0;


/* This is equivalent to afl-as.h: */

static unsigned char *afl_area_ptr; // bitmap

/* Set in the child process in forkserver mode: */

static unsigned char afl_fork_child;
unsigned int afl_forksrv_pid;

/* Instrumentation ratio: */

static unsigned int afl_inst_rms = MAP_SIZE;

/* get fuzzing inputs */
bool get_fuzz(uint8_t *buf, uint32_t size) {
    if(size && fuzz_cursor+size <= fuzz_size) {
        #ifdef DEBUG
        printf("[NATIVE FUZZ] Returning %d fuzz bytes\n", size); fflush(stdout);
        #endif
        memcpy(buf, &fuzz[fuzz_cursor], size);
        fuzz_cursor += size;
        return true;
    }
    else {
        #ifdef DEBUG
        printf("[NATIVE FUZZ] Running out of inputs \n"); fflush(stdout);
        #endif  
    }

    return false;
}

/* Determine if AFL or AFL++ input mode */
void determine_input_mode(void) {
    char *id_str;
    int shm_id;
    int tmp;

    id_str = getenv(SHM_FUZZ_ENV_VAR);
    if (id_str) {
        shm_id = atoi(id_str);
        // set up the fuzzing buffer allocated by AFL++
        fuzz = shmat(shm_id, NULL, 0);
        if (!fuzz || fuzz == (void *)-1) {
            perror("[!] could not access fuzzing shared memory");
            exit(1);
        }

        // AFL++ detected. Read its status value
        if(read(FORKSRV_FD, &tmp, 4) != 4) {
            perror("[!] did not receive AFL++ status value");
            exit(1);
        }

        input_mode_SHM = true;
    }
}


/* Set up SHM region and initialize other stuff. */
void afl_setup_bitmap(void) {
    char *id_str = getenv(SHM_ENV_VAR),
        *inst_r = getenv("AFL_INST_RATIO");
    int shm_id;

    if (inst_r) {
        unsigned int r;
        r = atoi(inst_r);
        if (r > 100) r = 100;
        if (!r) r = 1;
        afl_inst_rms = MAP_SIZE * r / 100;
    }

    if (id_str) {
        shm_id = atoi(id_str);
        afl_area_ptr = shmat(shm_id, NULL, 0);

        if (afl_area_ptr == (void*)-1) exit(1);

        /* With AFL_INST_RATIO set to a low value, we want to touch the bitmap
        so that the parent doesn't give up on us. */

        if (inst_r) afl_area_ptr[0] = 1;
    }

  /* pthread_atfork() seems somewhat broken in util/rcu.c, and I'm
     not entirely sure what is the cause. This disables that
     behaviour, and seems to work alright? */

    rcu_disable_atfork();
}

/* Fork server logic, invoked once we hit _start. */
void afl_forkserver(void) {
    static unsigned char tmp[4];
    uint8_t i = 0;
    if (!afl_area_ptr) return;

    /* Tell the parent that we're alive. If the parent doesn't want
        to talk, assume that we're not running in forkserver mode. */

    if (write(FORKSRV_FD + 1, tmp, 4) != 4) return;
    qemu_log_mask(LOG_GUEST_ERROR,
                      "%d: Debugging probe \n", i++);

    afl_forksrv_pid = getpid();

    /* All right, let's await orders... */

    while (1) {
        pid_t child_pid;
        int status, t_fd[2];

        /* Whoops, parent dead? */

        if (read(FORKSRV_FD, tmp, 4) != 4) exit(2);
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%d: Debugging probe \n", i++);
        /* Establish a channel with child to grab translation commands. We'll
        read from t_fd[0], child will write to TSL_FD. */

        if (pipe(t_fd) || dup2(t_fd[1], TSL_FD) < 0) exit(3);
        close(t_fd[1]);

        child_pid = fork();
        if (child_pid < 0) exit(4);

        qemu_log_mask(LOG_GUEST_ERROR,
                      "%d: Debugging probe: fork \n", i++);

        if (!child_pid) {

        /* Child process. Close descriptors and run free. */

        afl_fork_child = 1;
        close(FORKSRV_FD);
        close(FORKSRV_FD + 1);
        close(t_fd[0]);
        return;

        }

        /* Parent. */
        close(TSL_FD);

        if (write(FORKSRV_FD + 1, &child_pid, 4) != 4) exit(5);

        /* Collect translation requests until child dies and closes the pipe. */
        // afl_wait_tsl(cpu, t_fd[0]);

        /* Get and relay exit status to parent. */

        if (waitpid(child_pid, &status, 0) < 0) exit(6);
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%d: Debugging probe waitforpid: %d\n", i++, status);

        if (write(FORKSRV_FD + 1, &status, 4) != 4) exit(7);
    }
}

/* The equivalent of the tuple logging routine from afl-as.h. */
void afl_maybe_log(uint32_t cur_loc) {

    qemu_log_mask(LOG_GUEST_ERROR,
                      "bb 0x%x is accessed \n", cur_loc);

    /* crash if the execution falls into hardfault handler*/
    if (cur_loc == 0x80021c8) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Debugging probe kill at 0x80021c8 \n");
        kill(getpid(), SIGSEGV);
    }

    static __thread uint32_t prev_loc = 0;

    /* Optimize for cur_loc > afl_end_code, which is the most likely case on
        Linux systems. */

//   if (cur_loc > afl_end_code || cur_loc < afl_start_code || !afl_area_ptr)
//     return;

    if(!afl_area_ptr) return;

  /* Looks like QEMU always maps to fixed locations, so ASAN is not a
     concern. Phew. But instruction addresses may be aligned. Let's mangle
     the value to get something quasi-uniform. */

    cur_loc  = (cur_loc >> 4) ^ (cur_loc << 8);
    cur_loc &= MAP_SIZE - 1;

  /* Implement probabilistic instrumentation by looking at scrambled block
     address. This keeps the instrumented locations stable across runs. */

    if (cur_loc >= afl_inst_rms) return;    
    
    if (prev_loc != 0)
        afl_area_ptr[cur_loc ^ prev_loc]++;
    
    prev_loc = cur_loc >> 1;
}


/* This code is invoked whenever QEMU decides that it doesn't have a
   translation of a particular block and needs to compute it. When this happens,
   we tell the parent to mirror the operation, so that the next fork() has a
   cached copy. */

// void afl_request_tsl(CPUState *cpu, TCGTBCPUState s) {
// //  struct afl_tsl t;

//   if (!afl_fork_child) return;

// //   t.pc      = pc;
// //   t.cs_base = cb;
// //   t.flags   = flags;

//   if (write(TSL_FD, &s, sizeof(struct TCGTBCPUState)) != sizeof(struct TCGTBCPUState))
//     return;
// }

TranslationBlock *tb_htable_lookup(CPUState *cpu, TCGTBCPUState s);


/* This is the other side of the same channel. Since timeouts are handled by
   afl-fuzz simply killing the child, we can just wait until the pipe breaks. */
void afl_wait_tsl(CPUState *cpu, int fd) {
    TCGTBCPUState s;
    TranslationBlock *tb;

    while (1) {
        /* Broken pipe means it's time to return to the fork server routine. */
        if (read(fd, &s, sizeof(struct TCGTBCPUState)) != sizeof(struct TCGTBCPUState))
            break;

        tb = tb_htable_lookup(cpu, s);

        if(!tb) {
            mmap_lock();
            tb_lock_pages(tb);
            tb_gen_code(cpu, s);
            mmap_unlock();
            tb_unlock_pages(tb);
        }
    }
    close(fd);
}