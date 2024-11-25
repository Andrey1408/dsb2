#include "ipc.h"
#include "pipe_utils.h"
#include "message_logic.h"
#include "log.h"
#include "banking.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        return -1;
    }
    if (strcmp(argv[1], "-p") != 0)
    {
        return -1;
    }
    local_id process_num = atoi(argv[2]);
    if (argc < 3 + process_num)
        return -1;

    balance_t *balance = malloc((process_num + 1) * sizeof(balance_t));
    for (local_id i = 0; i < process_num; ++i)
    {
        balance[i + 1] = atoi(argv[3 + i]);
    }
    /*SOZDAT LOG*/
    FILE *pipes_log_file = fopen(pipes_log, "w+t");
    FILE *events_log_file = fopen(events_log, "w+t");
    pipe_ut *proc = (pipe_ut *)malloc(sizeof(pipe_ut));
    set_parent(proc, process_num + 1);

    create_pipes(proc, pipes_log_file);
    create_child_processes(proc, balance, events_log_file);

    if (proc->cur_id == PARENT_ID)
    {
        parent_work(proc, events_log_file);
    }
    else
    {
        child_work(proc, events_log_file);
    }
    // bank_robbery(parent_data);

    // print_history(all);
    fclose(pipes_log_file);
    fclose(events_log_file);
    destroyPipeline(proc);
    return 0;
}
