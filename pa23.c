#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include "ipc.h"
#include "pipe_utils.h"
#include "log.h"
#include "banking.h"



void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount)
{
    TransferOrder trnsfr = {
        .s_src = src, 
        .s_dst = dst, 
        .s_amount = amount
    };

    Message message;
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = sizeof(trnsfr);
    message.s_header.s_type = TRANSFER;
    message.s_header.s_local_time = get_physical_time();
    memcpy(message.s_payload, &trnsfr, sizeof(trnsfr));

    while (1) {
        if (send(parent_data, src, &message ) == -1) {
            continue;
        } else {
            break;
        }
    }

    log_transfer_out(&trnsfr);

    Message message_out;
    message_out.s_header.s_type = STARTED;
    while (message_out.s_header.s_type != ACK) {
        receive(parent_data, dst, &message_out);
    }

    log_transfer_in(&trnsfr);
}

void balance_history (BalanceHistory* history, BalanceState state) {
    
    if (state.s_time >= history->s_history_len -1) {
        for( timestamp_t t = history->s_history_len; t < state.s_time; t++ ) {
            history->s_history[t] = history->s_history[t-1];
            history->s_history[t].s_time = t;
        }
        history->s_history[state.s_time] = state;
        history->s_history_len = state.s_time + 1;
    }
    if (state.s_time < history->s_history_len -1) {
        return;
    }
}

void wait_messages(pipe_ut* pp, MessageType status) 
{
    local_id counter = 0;
    local_id to_wait;
    if (pp->cur_id == PARENT_ID) {
        to_wait = pp->size - 1;
    } else {
        to_wait = pp->size - 2;
    }
    
    while (counter < to_wait) {
        Message msg;
        receive_any( pp, &msg );
        if (msg.s_header.s_type == status) {
            counter++;
        }
    } 
}

Message create_message ( MessageType type, void* contents, uint16_t size ) 
{
    Message msg;

    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = 0;
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = get_physical_time();
    msg.s_header.s_payload_len = size;
    memcpy(&(msg.s_payload), contents, size);

    return msg;
}


void child_work(pipe_ut* pp) 
{
    Message msg = create_message(STARTED, pp, 0);
    send(pp, PARENT_ID,  &msg);
    timestamp_t end_time = 0;

    for (int i = 0; i < pp->size; i++) {
        Message msg;
        receive_any(pp, &msg);

        switch (msg.s_header.s_type)
        {
        case TRANSFER:
            break;
        case STOP:
            break;
        case DONE:
            break;
        default:
            break;
        }
    }
   

}

int main(int argc, char * argv[])
{
    //bank_robbery(parent_data);
    //print_history(all);

    return 0;
}
