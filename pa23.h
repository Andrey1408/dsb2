#ifndef PA23_DSB
#define PA23_DSB

#include "ipc.h"
#include "pipe_utils.h"
#include "log.h"
#include "banking.h"
#include "message_logic.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

void child_stopping(pipe_ut *pp, const int *processes_left_counter, FILE *events_log_file);
void child_work(pipe_ut *pp, FILE *events_log_file);
void parent_work(pipe_ut *pp, FILE *events_log_file);
void set_parent(pipe_ut *proc, local_id size);
void create_child_processes(pipe_ut *proc, balance_t *balance, FILE *log);
#endif
