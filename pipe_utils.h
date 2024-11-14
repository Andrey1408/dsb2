#ifndef CUSTOM_PIPE_UTILS_DSB2
#define CUSTOM_PIPE_UTILS_DSB2

#include "log.h"
#include "ipc.h"
#include "banking.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
typedef struct pipe_ut
{
    local_id cur_id;
    local_id size;
    int ***recepients;
    BalanceState state;
    BalanceHistory history;
} pipe_ut;

void create_pipes(pipe_ut *proc, FILE *pipes_log_file);
void close_unused_pipes(pipe_ut *proc);
void close_write_pipe_ends(pipe_ut *proc);
void close_read_pipe_ends(pipe_ut *proc);
int getWriterById(local_id self_id, local_id dest, pipe_ut *proc);
int getReaderById(local_id self_id, local_id dest, pipe_ut *proc);
void destroyPipeline(pipe_ut *proc);

#endif
