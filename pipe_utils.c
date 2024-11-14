#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "log.h"
#include "pipe_utils.h"

void create_pipes(pipe_ut *proc, FILE *pipes_log_file)
{
    proc->recepients = (int ***)malloc(sizeof(int **) * proc->size);
    for (int i = 0; i < proc->size; i++)
    {
        proc->recepients[i] = (int **)malloc(sizeof(int *) * proc->size);
        for (int j = 0; j < proc->size; j++)
        {
            proc->recepients[i][j] = (int *)malloc(sizeof(int) * 2 * proc->size);
            // proc->pipes[i][j][0] = (int)malloc(sizeof(int));
            // proc->pipes[i][j][1] = (int)malloc(sizeof(int));
            if (i != j)
            {
                log_pipe(pipes_log_file, i, j, proc->recepients[i][j][0], proc->recepients[i][j][1]);
                pipe(proc->recepients[i][j]);
            }
            else
            {
                proc->recepients[i][j][0] = -1;
                proc->recepients[i][j][1] = -1;
            }
        }
    }
}



void close_write_pipe_ends(pipe_ut *proc)
{
    for (local_id j = 0; j < proc->size; j++)
    {
        if (proc->cur_id != j)
        {
            close(proc->pipes[proc->cur_id][j][1]);
        }
    }
}

void close_read_pipe_ends(pipe_ut *proc)
{
    for (local_id j = 0; j < proc->size; j++)
    {
        if (proc->cur_id != j)
        {
            close(proc->pipes[proc->cur_id][j][0]);
        }
    }
}