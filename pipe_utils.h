#include "ipc.h"
#include "banking.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct 
{
    local_id cur_id;
    local_id size;
    int ***recepients;
    BalanceState state;
    BalanceHistory history;
} pipe_ut;

void create_pipes( pipe_ut* proc, FILE *pipes_log_file );
void close_unused_pipes ( pipe_ut* proc );
void close_write_pipe_ends(pipe_ut *proc);
void close_read_pipe_ends(pipe_ut *proc);
int getWriterById(local_id self_id, local_id dest, pipe_ut *proc);
int getReaderById(local_id self_id, local_id dest, pipe_ut *proc);
