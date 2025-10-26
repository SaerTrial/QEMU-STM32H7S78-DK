#include "../patches/afl.h"

extern uint32_t afl_start_code, afl_end_code;
static bool input_already_given = false;
extern bool input_mode_SHM;
uint8_t *fuzz = NULL;
uint32_t fuzz_size = 0;
uint32_t fuzz_cursor = 0;
uint32_t startForkserver(CPUArchState *env, uint32_t enableTicks);
uint32_t getWork(CPUArchState *env, uint32_t ptr, uint32_t sz);
uint32_t startWork(CPUArchState *env, uint32_t ptr);
uint32_t doneWork(uint32_t val);
const char *peekStrZ(CPUArchState *env, uint32_t ptr, int maxlen);
bool getFuzz(uint8_t *buf, uint32_t size);
static void loadFuzz(void);
QDict * load_configuration(const char * filename);


void loadFuzz(void) {
    FILE *fp;
    
    assert(aflStart == 1);
    
    if (input_mode_SHM){
        fuzz_size = (*(uint32_t *)fuzz) + sizeof(uint32_t);
        fuzz_cursor = sizeof(uint32_t);
        qemu_log_mask(LOG_GUEST_ERROR,
                       "%s: fuzz_size:0x%x fuzz_cursor:0x%x \n", __func__, (uint32_t)fuzz_size, fuzz_cursor);
        return ;
    }

    fp = fopen(aflFile, "rb");
    if(!fp) {
         perror(aflFile);
         exit(1);
    }

    fseek(fp, 0L, SEEK_END);
    uint32_t _size = ftell(fp);
    fuzz_size = _size;
    fseek(fp, 0L, SEEK_SET); 

    if ( (fuzz = malloc(_size+1)) == NULL ) {
        exit(1);
    }

    if (fread(fuzz, _size, 1, fp) == 0) {
        printf("fread failed\n");
        exit(1);
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                       "%s: read fuzzing input file successfully, size:0x%x\n", __func__, (uint32_t)_size);

    fclose(fp);
}


/* get fuzzing inputs */
bool getFuzz(uint8_t *buf, uint32_t size) {
    if(!input_already_given){
        loadFuzz();
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
        if(!input_mode_SHM) free(fuzz);
        doneWork(0);
    }

    return false;
}

uint32_t startForkserver(CPUArchState *env, uint32_t enableTicks)
{
    qemu_log_mask(LOG_GUEST_ERROR, "[startForkserver] PanicAddr: 0x%lx DmsgAddr:0x%lx \n", aflPanicAddr, aflDmesgAddr); 

    //printf("pid %d: startForkServer\n", getpid()); fflush(stdout);
    if(afl_fork_child) {
        /* 
         * we've already started a fork server. perhaps a test case
         * accidentally triggered startForkserver again.  Exit the
         * test case without error.
         */
        exit(0);
    }
#ifdef CONFIG_USER_ONLY
    /* we're running in the main thread, get right to it! */
    afl_setup();
    afl_forkserver(env);
#else
    /*
     * we're running in a cpu thread. we'll exit the cpu thread
     * and notify the iothread.  The iothread will run the forkserver
     * and in the child will restart the cpu thread which will continue
     * execution.
     * N.B. We assume a single cpu here!
     */
    aflEnableTicks = enableTicks;
    afl_wants_cpu_to_stop = 1;
#endif
    return 0;
}

/* copy work into ptr[0..sz].  Assumes memory range is locked. */
uint32_t getWork(CPUArchState *env, uint32_t ptr, uint32_t sz)
{
    return 0;
    // uint32_t retsz;
    // FILE *fp;
    // unsigned char ch;
    // qemu_log_mask(LOG_GUEST_ERROR, "get_work:\n");
    // //printf("pid %d: getWork %lx %lx\n", getpid(), ptr, sz);fflush(stdout);
    // assert(aflStart == 0);
    // fp = fopen(aflFile, "rb");
    // if(!fp) {
    //      perror(aflFile);
    //      return -1;
    // }
    // retsz = 0;
    // while(retsz < sz) {
    //     if(fread(&ch, 1, 1, fp) == 0)
    //         break;
    //     qemu_log_mask(LOG_GUEST_ERROR, "%c", ch);
    //     cpu_stb_data(env, ptr, ch);
    //     retsz ++;
    //     ptr ++;
    // }
    // qemu_log_mask(LOG_GUEST_ERROR, "\n");
    // fclose(fp);
    // return retsz;
}

uint32_t startWork(CPUArchState *env, uint32_t ptr)
{
    //uint32_t start, end;
    //input_already_given = false;
    // qemu_log("pid %d: ptr %lx\n", getpid(), ptr);
    // start = cpu_ldq_data(env, ptr);
    // end = cpu_ldq_data(env, ptr + sizeof start);
    // qemu_log("pid %d: startWork %x - %x\n", getpid(), start, end);

    // afl_start_code = start;
    // afl_end_code   = end;
    // aflGotLog = 0;
    // aflStart = 1;
    // qemu_log("aflStart = %d\n", aflStart);
    return 0;
}

uint32_t doneWork(uint32_t val)
{
    //printf("pid %d: doneWork %lx\n", getpid(), val);fflush(stdout);
    assert(aflStart == 1);
/* detecting logging as crashes hasnt been helpful and
   has occasionally been a problem.  We'll leave it to
   a post-analysis phase to look over dmesg output for
   our corpus.
 */
#ifdef LETSNOT 
    if(aflGotLog)
        exit(64 | val);
#endif
    _exit(val); /* exit forkserver child */
}

uint32_t helper_aflCall32(CPUArchState *env, uint32_t code, uint32_t a0, uint32_t a1) {
    return (uint32_t)helper_aflCall(env, code, a0, a1);
}

uint64_t helper_aflCall(CPUArchState *env, uint64_t code, uint64_t a0, uint64_t a1) {
    qemu_log_mask(LOG_GUEST_ERROR, "helper_aflCall(%p, %ld, %ld, %ld)\n", env, code, a0, a1);
    switch(code) {
    case 1: return startForkserver(env, a0);
    case 2: return getWork(env, a0, a1);
    case 3: return startWork(env, a0);
    case 4: return doneWork(a0);
    default: return -1;
    }
}

/* return pointer to buf filled with strz from ptr[0..maxlen] */
const char *
peekStrZ(CPUArchState *env, uint32_t ptr, int maxlen)
{
    // static char buf[0x1000];
    // int i;
    // if(maxlen > sizeof buf - 1)
    //     maxlen = sizeof buf - 1;
    // for(i = 0; i < maxlen; i++) {
    //     char ch = cpu_ldub_data(env, ptr + i);
    //     if(!ch)
    //         break;
    //     buf[i] = ch;
    // }
    // buf[i] = 0;
    // return buf;
    return NULL;
}

void helper_aflInterceptLog(CPUArchState *env)
{
    if(!aflStart)
        return;
    aflGotLog = 1;
}

void helper_aflInterceptPanic(void)
{
    if(!aflStart)
        return;
    qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: aflPanicAddr \n", __func__);
    exit(32);
}

QDict * load_configuration(const char * filename)
{
    int file = open(filename, O_RDONLY);
    off_t filesize = lseek(file, 0, SEEK_END);
    char * filedata = NULL;
    ssize_t err;
    QObject * obj;

    lseek(file, 0, SEEK_SET);

    filedata = g_malloc(filesize + 1);
    memset(filedata, 0, filesize + 1);

    if (!filedata)
    {
        fprintf(stderr, "%ld\n", filesize);
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    err = read(file, filedata, filesize);

    if (err != filesize)
    {
        fprintf(stderr, "Reading configuration file failed\n");
        exit(1);
    }

    close(file);

    obj = qobject_from_json(filedata, NULL);
    if (!obj || qobject_type(obj) != QTYPE_QDICT)
    {
        fprintf(stderr, "Error parsing JSON configuration file\n");
        exit(1);
    }

    g_free(filedata);

    return qobject_to(QDict, obj);
}
