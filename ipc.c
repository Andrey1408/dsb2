#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "ipc.h"
#include "pipe_utils.h"

int send(void * self, local_id dst, const Message * msg) 
{
    pipe_ut * proc = self;
    if (write(proc->writer[proc->cur_id][dst], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) != -1) {
        return 0;
    }
    return -1;
}

int send_multicast(void * self, const Message * msg) 
{
    pipe_ut * proc = self;
    for ( local_id i = 0; i < proc->size; i++ ) {
        if ( i != proc->cur_id ) {
            if ( send( proc, i, msg ) == -1 ) {
                return -1;
            }
        }
            
    }
    return 0;
}

int receive(void * self, local_id from, Message * msg) 
{
    pipe_ut * proc = self;
    if (read(proc->writer[proc->cur_id][from], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) != -1 || read(proc->writer[proc->cur_id][from], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) != 0) {
        return 0;
    }
    return -1;
}

int receive_any(void * self, Message * msg) 
{
    pipe_ut * proc = self;
    while (1) {
        for ( local_id i = 0; i < proc->size; i++ ) {
            if ( i != proc->cur_id ) {
                if ( receive( self, i, msg ) != -1 ) {
                    return 0;
                }               
            }
        } 
    }
}