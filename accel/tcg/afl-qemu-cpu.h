#ifndef AFL_QEMU_COMMON_H
#define AFL_QEMU_COMMON_H

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/rcu.h"
#include "accel/tcg/tb-cpu-state.h"
#include "accel/tcg/internal-common.h"
#include "accel/tcg/tb-internal.h"
#include "exec/mmap-lock.h"

#include <sys/shm.h>


// AFL-related constants
// 65k bitmap size
#define MAP_SIZE_POW2       16
#define MAP_SIZE            (1 << MAP_SIZE_POW2)
#define FORKSRV_FD          198
#define SHM_ENV_VAR         "__AFL_SHM_ID"
// AFL++ compatibility constants
#define SHM_FUZZ_ENV_VAR "__AFL_SHM_FUZZ_ID"
#define FS_OPT_SHDMEM_FUZZ 0x01000000
#define FS_OPT_ENABLED 0x80000001

#define TSL_FD (FORKSRV_FD - 1)

// #define DEBUG
void afl_reset_cov(void);
bool afl_load_fuzz(void);
bool afl_get_fuzz(uint8_t *buf, uint32_t size);
void afl_determine_input_mode(void);
void afl_init_bitmap(void);
void afl_forkserver(void);
void afl_maybe_log(uint32_t cur_loc);
void afl_wait_tsl(CPUState *cpu, int fd);
#endif