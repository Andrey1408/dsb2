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
            if (i != j)
            {
                log_pipe(pipes_log_file, i, j, proc->recepients[i][j][0], proc->recepients[i][j][1]);
                pipe(proc->recepients[i][j]);
                fcntl(proc->recepients[i][j][0], F_SETFL, O_NONBLOCK);
                fcntl(proc->recepients[i][j][1], F_SETFL, O_NONBLOCK);
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
            close(proc->recepients[proc->cur_id][j][1]);
        }
    }
}

void close_read_pipe_ends(pipe_ut *proc)
{
    for (local_id j = 0; j < proc->size; j++)
    {
        if (proc->cur_id != j)
        {
            close(proc->recepients[proc->cur_id][j][0]);
        }
    }
}

int getWriterById(local_id self_id, local_id dest, pipe_ut *proc)
{

    return proc->recepients[self_id][dest][1];
}

int getReaderById(local_id self_id, local_id dest, pipe_ut *proc)
{

    return proc->recepients[self_id][dest][0];
}

void destroyPipeline(pipe_ut *proc)
{
    for (int i = 0; i < proc->size; i++)
    {
        for (int j = 0; j < proc->size; j++)
        {
            free(proc->recepients[i][j]);
        }
        free(proc->recepients[i]);
    }
    free(proc->recepients);
}
