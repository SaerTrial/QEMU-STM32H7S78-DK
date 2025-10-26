#ifndef __AFL_H
#define __AFL_H
#include "qobject/qdict.h"
#include "qobject/qjson.h"
#include "qobject/qnull.h"
#include "qobject/qstring.h"
#include "qobject/qlist.h"
extern const char *aflConf;
extern const char *aflFile;
extern unsigned long aflPanicAddr;
extern unsigned long aflDmesgAddr;

extern uint32_t aflEnabled;
extern int aflEnableTicks;
extern int aflStart;
extern int aflGotLog;
extern unsigned char afl_fork_child;
extern int afl_wants_cpu_to_stop;

extern int afl_qemuloop_pipe[2];
extern CPUState *restart_cpu;

void afl_setup(void);
void afl_forkserver(CPUArchState*);
#endif