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
    int **reader;
    int **writer;
    BalanceState state;
    BalanceHistory history;
} pipe_ut;

void create_pipes( pipe_ut* proc, FILE *pipes_log_file );
void close_unused_pipes ( pipe_ut* proc );