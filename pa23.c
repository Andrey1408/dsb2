#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include "ipc.h"
#include "pipe_utils.h"
#include "log.h"
#include "banking.h"

Message create_message(MessageType type, void *contents, uint16_t size)
{
    Message msg;

    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_local_time = get_physical_time();
    msg.s_header.s_payload_len = size;
    memcpy(&(msg.s_payload), contents, size);

    return msg;
}

void transfer(void *parent_data, local_id src, local_id dst,
              balance_t amount)
{
    TransferOrder trnsfr = {
        .s_src = src,
        .s_dst = dst,
        .s_amount = amount};

    Message message = create_message(TRANSFER, (void *)&trnsfr, sizeof(trnsfr));

    send(parent_data, src, &message);

    Message message_out;
    message_out.s_header.s_type = STARTED;
}

void balance_history(BalanceHistory *history, BalanceState state)
{

    if (state.s_time >= history->s_history_len - 1)
    {
        for (timestamp_t t = history->s_history_len; t < state.s_time; t++)
        {
            history->s_history[t] = history->s_history[t - 1];
            history->s_history[t].s_time = t;
        }
        history->s_history[state.s_time] = state;
        history->s_history_len = state.s_time + 1;
    }
    if (state.s_time < history->s_history_len - 1)
    {
        return;
    }
}

void wait_messages(pipe_ut *pp, MessageType status)
{
    local_id counter = 0;
    local_id to_wait;
    if (pp->cur_id == PARENT_ID)
    {
        to_wait = pp->size - 1;
    }
    else
    {
        to_wait = pp->size - 2;
    }

    while (counter < to_wait)
    {
        Message msg;
        receive_any(pp, &msg);
        if (msg.s_header.s_type == status)
        {
            counter++;
        }
    }
}

void transfer_process(pipe_ut *pp, Message *msg)
{
    TransferOrder *trnsfr = (TransferOrder *)msg->s_payload;
    if (pp->cur_id == trnsfr->s_dst)
    {
        pp->state.s_balance += trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        send(pp, trnsfr->s_dst, msg);
        log_transfer_out(trnsfr);
    }
    if (pp->cur_id == trnsfr->s_src)
    {
        pp->state.s_balance -= trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        Message msg = create_message(ACK, NULL, 0);
        send(pp, PARENT_ID, &msg);
        log_transfer_in(trnsfr);
    }
}

void child_stopping(pipe_ut *pp, const int *processes_left_counter, FILE *events_log_file)
{
    Message msg = create_message(DONE, NULL, 0);
    send_multicast((void *)pp, &msg);
    int i = *processes_left_counter;
    for (int id = 1; i < pp->size - 2 || id < pp->size - 1; id++)
    {
        if (id != pp->cur_id)
        {
            int status = receive(pp, id, &msg);
            if (status == 0)
            {
                switch (msg.s_header.s_type)
                {
                case DONE:
                    i++;
                    continue;
                case TRANSFER:
                    pp->state.s_time = get_physical_time();
                    transfer_process(pp, &msg);
                    id = 0;
                    continue;
                default:
                    continue;
                }
            }
        }
    }
    msg = create_message(DONE, NULL, 0);
    send_multicast((void *)pp, &msg);
    timestamp_t end_time = get_physical_time();
    pp->state = pp->history.s_history[pp->history.s_history_len - 1];
    pp->state.s_time = end_time;
    balance_history(&(pp->history), pp->state);
    log_done(pp->cur_id, events_log_file);
    uint16_t p_size = (pp->history.s_history_len) * sizeof(BalanceState) + sizeof(pp->history.s_history_len) + sizeof(pp->history.s_id);
    msg = create_message(BALANCE_HISTORY, &pp->history, p_size);
    send(pp, PARENT_ID, &msg);
}

void child_work(pipe_ut *pp, FILE *events_log_file)
{
    Message msg = create_message(STARTED, NULL, 0);
    send(pp, PARENT_ID, &msg);
    int i = 0;
    while (i < pp->size - 2)
    {
        Message msg;
        receive_any(pp, &msg);

        switch (msg.s_header.s_type)
        {
        case TRANSFER:
            pp->state.s_time = get_physical_time();
            transfer_process(pp, &msg);
            continue;
        case STOP:
            child_stopping(pp, &i, events_log_file);
            break;
        case DONE:
            i++;
            continue;
        default:
            continue;
        }
    }
}

int main(int argc, char *argv[])
{

    // bank_robbery(parent_data);
    // print_history(all);

    return 0;
}
