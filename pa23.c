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
        for ( timestamp_t t = history->s_history_len; t < state.s_time; t++ ) {
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

void transfer_process(pipe_ut* pp, Message* msg) 
{
    TransferOrder* trnsfr = (TransferOrder*)msg->s_payload;
    if (pp->cur_id == trnsfr->s_dst) {
        pp->state.s_balance += trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        send(pp, trnsfr->s_dst, msg);
    }
    if (pp->cur_id == trnsfr->s_src) {
        pp->state.s_balance -= trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        Message msg = create_message(ACK, NULL, 0);
        send(pp, PARENT_ID, &msg);
    }
}

void child_work(pipe_ut* pp) 
{
    Message msg = create_message(STARTED, NULL, 0);
    send(pp, PARENT_ID,  &msg);
    timestamp_t end_time = 0;
    int i = 0;
    while (i < pp->size - 2) {
        Message msg;
        receive_any(pp, &msg);

        switch (msg.s_header.s_type)
        {
        case TRANSFER:
            pp->state.s_time = get_physical_time();
            transfer_process(pp, &msg);
            break;
        case STOP:
            Message msg = create_message(DONE, NULL, 0);
            send_multicast(pp, &msg);
            break;
        case DONE:
            i++;
            break;
        default:
            break;
        }
    }
    pp->state = pp->history.s_history[pp->history.s_history_len-1];
    pp->state.s_time = end_time;
    add_balance_state_to_history(&(pp->history), pp->state);
    log_done(pp);
    uint16_t p_size = (pp->history.s_history_len) * sizeof(BalanceState) + sizeof(pp->history.s_history_len) + sizeof(pp->history.s_id);
    Message msg_to_p = create_message(BALANCE_HISTORY, &pp->history, p_size);
    send(pp, PARENT_ID, &msg_to_p);
}

int main(int argc, char * argv[])
{
    //bank_robbery(parent_data);
    //print_history(all);

    return 0;
}
