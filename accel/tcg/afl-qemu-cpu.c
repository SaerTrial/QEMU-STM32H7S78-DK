
#include "afl-qemu-cpu.h"

uint8_t *fuzz = NULL;
bool input_mode_SHM = false;
long fuzz_size = 0;
long fuzz_cursor = 0;

#define DEBUG

/* This is equivalent to afl-as.h: */

static unsigned char *bitmap; // bitmap

/* Set in the child process in forkserver mode: */

static unsigned char afl_fork_child;
unsigned int afl_forksrv_pid;
__thread uint32_t prev_loc = 0;

/* Instrumentation ratio: */

static unsigned int afl_inst_rms = MAP_SIZE;
static bool input_already_given = false;


void afl_reset_cov(void) {
    return ;

    memset(bitmap, 0, MAP_SIZE);
    bitmap[0] = 1;
    prev_loc = 0;
    input_already_given = false;

}


/* get fuzzing inputs */
bool afl_get_fuzz(uint8_t *buf, uint32_t size) {

    if(!input_already_given){
        afl_load_fuzz();
        input_already_given = true;
    }

    if(size && fuzz_cursor + size <= fuzz_size) {
        qemu_log_mask(LOG_GUEST_ERROR, "[NATIVE FUZZ] Returning %d fuzz bytes\n", size); 
        memcpy(buf, &fuzz[fuzz_cursor], size);
        fuzz_cursor += size;
        return true;
    }
    else {
        qemu_log_mask(LOG_GUEST_ERROR, "[NATIVE FUZZ] Running out of inputs \n"); 
        // exit(0);
        _exit(0);
        // raise(SIGSTOP);
    }

    return false;
}

/* Determine if AFL or AFL++ input mode */
void afl_determine_input_mode(void) {
    char *id_str;
    int shm_id;
    int tmp;

    #ifdef DEBUG
        qemu_log_mask(LOG_GUEST_ERROR,
                    "afl_determine_input_mode \n");
    #endif

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
void afl_init_bitmap(void) {
    #ifdef DEBUG
    qemu_log_mask(LOG_GUEST_ERROR,
                    "afl_init_bitmap \n");
    #endif

    uint32_t tmp = FS_OPT_ENABLED | FS_OPT_SHDMEM_FUZZ;

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
        bitmap = shmat(shm_id, NULL, 0);

        if (bitmap == (void*)-1) {
            qemu_log_mask(LOG_GUEST_ERROR,
                    "bitmap == -1 \n");
            exit(1);
        }

        if (write(FORKSRV_FD + 1, &tmp, 4) != 4) {
            qemu_log_mask(LOG_GUEST_ERROR,
                "enable SHM fuzzing failed\n");
            exit(1);
        }

        /* With AFL_INST_RATIO set to a low value, we want to touch the bitmap
        so that the parent doesn't give up on us. */

        if (inst_r) bitmap[0] = 1;
    }

  /* pthread_atfork() seems somewhat broken in util/rcu.c, and I'm
     not entirely sure what is the cause. This disables that
     behaviour, and seems to work alright? */

    // rcu_disable_atfork();
}


bool afl_load_fuzz(void) {

    if(input_mode_SHM) {
        // shm inputs: <size_u32> contents ...
        fuzz_size = (*(uint32_t *)fuzz) + sizeof(uint32_t);
        fuzz_cursor = sizeof(uint32_t);

        #ifdef DEBUG
        qemu_log_mask(LOG_GUEST_ERROR,
            "afl_load_fuzz: fuzz_size:0x%lx fuzz_cursor:0x%lx..\n", fuzz_size, fuzz_cursor);
        #endif

        return true;
    }
    return false;
}

/* Fork server logic, invoked once we hit _start. */
void afl_forkserver(void) {
    #ifdef DEBUG
        qemu_log_mask(LOG_GUEST_ERROR,
                    "afl_forkserver \n");
    #endif

    static uint32_t was_killed;

    if (!bitmap) return;

    afl_forksrv_pid = getpid();

    /* All right, let's await orders... */

    while (1) {
        pid_t child_pid;
        int status;
        #ifdef TRANSLATE_OPT
        int t_fd[2];
        #endif

        /* Whoops, parent dead? */
        
        if (read(FORKSRV_FD, &was_killed, 4) != 4) {
            qemu_log_mask(LOG_GUEST_ERROR,
                      "forkserver before fork, read status failed \n");
            exit(2);
        }

        afl_reset_cov();

        /* Establish a channel with child to grab translation commands. We'll
        read from t_fd[0], child will write to TSL_FD. */

        #ifdef TRANSLATE_OPT
        if (pipe(t_fd) || dup2(t_fd[1], TSL_FD) < 0) exit(3);
        close(t_fd[1]);
        #endif

        child_pid = fork();

        if (child_pid < 0) {
            qemu_log_mask(LOG_GUEST_ERROR,
            "fork a child failed\n");
            exit(4);
        }

        if (!child_pid) {
            /* Child process. Close descriptors and run free. */
            afl_fork_child = 1;
            close(FORKSRV_FD);
            close(FORKSRV_FD + 1);

            #ifdef TRANSLATE_OPT
            close(t_fd[0]);
            #endif

            // sleep(50);
            return;
        }

        /* Parent. */
        // close(TSL_FD);

        if (write(FORKSRV_FD + 1, &child_pid, 4) != 4) {
            qemu_log_mask(LOG_GUEST_ERROR,
                      "forkserver: write child pid failed \n");
            exit(5);
        }

        /* Collect translation requests until child dies and closes the pipe. */
        // afl_wait_tsl(cpu, t_fd[0]);

        /* Get and relay exit status to parent. */

        fprintf(stderr, "waiting for child status, child pid:%d \n", child_pid);
        if (waitpid(child_pid, &status, 0) < 0) {
             qemu_log_mask(LOG_GUEST_ERROR,
                      "wait for child failed \n");
            exit(6);
        }
       
        #ifdef DEBUG
        qemu_log_mask(LOG_GUEST_ERROR,
                "child status: %d child pid: %d\n", status, child_pid);
                
        #endif

        // fprintf(stderr, "write status to fuzzer\n");

        if (write(FORKSRV_FD + 1, &status, 4) != 4){ 
             qemu_log_mask(LOG_GUEST_ERROR,
                      "write child status failed \n");
            
            fprintf(stderr, "write child status failed\n");

            exit(7);
        }

        // fprintf(stderr, "write child status successful\n");

    }
}

/* The equivalent of the tuple logging routine from afl-as.h. */
void afl_maybe_log(uint32_t cur_loc) {

    #ifdef DEBUG
    qemu_log_mask(LOG_GUEST_ERROR,
                      "bb 0x%x is accessed \n", cur_loc);
    #endif

    /* crash if the execution falls into hardfault handler*/
    if (cur_loc == 0x08002208) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Debugging probe kill at 0x80021c8 \n");
        _exit(SIGSEGV);
        // kill(getpid(), SIGSEGV);
    }


    /* Optimize for cur_loc > afl_end_code, which is the most likely case on
        Linux systems. */

//   if (cur_loc > afl_end_code || cur_loc < afl_start_code || !bitmap)
//     return;

    if(!bitmap) return;

  /* Looks like QEMU always maps to fixed locations, so ASAN is not a
     concern. Phew. But instruction addresses may be aligned. Let's mangle
     the value to get something quasi-uniform. */

    cur_loc  = (cur_loc >> 4) ^ (cur_loc << 8);
    cur_loc &= MAP_SIZE - 1;

  /* Implement probabilistic instrumentation by looking at scrambled block
     address. This keeps the instrumented locations stable across runs. */

    if (cur_loc >= afl_inst_rms) return;    
    
    if (prev_loc != 0){
        qemu_log_mask(LOG_GUEST_ERROR,
                      "edge key: 0x%x\n", cur_loc ^ prev_loc);
        bitmap[cur_loc ^ prev_loc]++;
    }
    
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