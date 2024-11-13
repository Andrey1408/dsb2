#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "log.h"
#include "pipe_utils.h"

void create_pipes( pipe_ut* proc, FILE *pipes_log_file ) 
{
    proc->reader = (int **)malloc( sizeof(int *) * proc->size );
    proc->writer = (int **)malloc( sizeof(int *) * proc->size );
    proc->reader[0] = malloc( sizeof(int) * (proc->size * proc->size) );
    proc->writer[0] = malloc( sizeof(int) * (proc->size * proc->size) );
    for ( int i = 0; i < proc->size; i++ ) {
        proc->reader[i] = ( *(proc->reader) + proc->size * i );
        proc->writer[i] = ( *(proc->writer) + proc->size * i );
    }

    for ( local_id i = 0; i < proc->size; i++ ) {
        for ( local_id j = 0; j < proc->size; j++ ) {
            if ( j != i ) {
                int pipes[2];
                log_pipe(pipes_log_file, i, j, proc->reader[i][j], proc->writer[i][j]);
                proc->reader[j][i] = pipes[0];
                proc->writer[i][j] = pipes[1];
            }
        }
    }
}

void close_unused_pipes ( pipe_ut* proc ) 
{
    for ( local_id i = 0; i < proc->size; i++ ) {
        for ( local_id j = 0; j < proc->size; j++ ) {
            if ( i != proc->cur_id && i != j ) {
                close( proc->reader[i][j] );
                close( proc->writer[i][j] );
            }
        }
    }
}

