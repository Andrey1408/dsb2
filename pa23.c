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
    TransferOrder trnsfr;
    trnsfr.s_src = src;
    trnsfr.s_dst = dst;
    trnsfr.s_amount = amount;
    // printf("TRANSFER %d - %d SRC TO %d -%d DST %d - %d AMOUNT\n", src, trnsfr->s_src, dst, trnsfr->s_dst, (int) amount, (int)trnsfr->s_amount);

    Message message = create_message(TRANSFER, (void *)&trnsfr, sizeof(TransferOrder));

    send(parent_data, src, &message);

    Message message_out;
    message_out.s_header.s_type = STARTED;

    while ( message_out.s_header.s_type != ACK ) {
        receive( parent_data, dst, &message_out );
    }
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

void transfer_process(pipe_ut *pp, Message *msg, FILE *events_log_file)
{
    TransferOrder *trnsfr = (TransferOrder *)msg->s_payload;
    if (pp->cur_id == trnsfr->s_src)
    if (pp->cur_id == trnsfr->s_src)
    {
        pp->state.s_balance += trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        send(pp, trnsfr->s_dst, msg);
        log_transfer_out(trnsfr, events_log_file);
    }
    if (pp->cur_id == trnsfr->s_dst)
    {
        pp->state.s_balance -= trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        Message msg = create_message(ACK, NULL, 0);
        send(pp, PARENT_ID, &msg);
        log_transfer_in(trnsfr, events_log_file);
    }
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
        printf("child %d received: %d \n", pp->cur_id, msg.s_header.s_type);
        switch (msg.s_header.s_type)
        {
            case TRANSFER:
                pp->state.s_time = get_physical_time();
                transfer_process(pp, &msg, events_log_file);
                break;
            case STOP:
                msg = create_message(DONE, NULL, 0);
                send_multicast( pp, &msg );
                break;
            case DONE:
                i++;
                break;
            default:
                printf("default broke in child work");
                break;
        }
    }
    Message msg_done = create_message(DONE, NULL, 0);
    send_multicast((void *)pp, &msg_done);
    timestamp_t end_time = get_physical_time();
    pp->state = pp->history.s_history[pp->history.s_history_len - 1];
    pp->state.s_time = end_time;
    balance_history(&(pp->history), pp->state);
    log_done(pp->cur_id, events_log_file);
    uint16_t p_size = (pp->history.s_history_len) * sizeof(BalanceState) + sizeof(pp->history.s_history_len) + sizeof(pp->history.s_id);
    msg_done = create_message(BALANCE_HISTORY, &pp->history, p_size);
    send(pp, PARENT_ID, &msg_done);
}
void parent_work(pipe_ut *pp, FILE *events_log_file)
{
    wait_messages(pp, STARTED);
    log_received_all_started(events_log_file, pp->cur_id);
    bank_robbery(pp, pp->size);
    Message msg = create_message(STOP, NULL, 0);
    send_multicast(pp, &msg);
    wait_messages(pp, DONE);
    log_received_all_done(events_log_file, pp->cur_id);
    AllHistory history;
    history.s_history_len = 0;
    while (history.s_history_len < pp->size - 1)
    {
        printf("in parent receiving history\n");
        receive_any(pp, &msg);
        if (msg.s_header.s_type == BALANCE_HISTORY)
        {
            BalanceHistory temp;
            memcpy(&temp, &(msg.s_payload), sizeof(msg.s_payload));
            history.s_history[temp.s_id - 1] = temp;
            history.s_history_len++;
        }
    }
    print_history(&history);
}

void set_parent(pipe_ut *proc, local_id size)
{
    proc->cur_id = PARENT_ID;
    proc->size = size;
}

void create_child_processes(pipe_ut *proc, balance_t *balance, FILE *log)
{
    for (local_id i = 0; i < proc->size; i++)
    {
        if (i != PARENT_ID && proc->cur_id == PARENT_ID)
        {
            pid_t p = fork();
            if (p == 0)
            {
                proc->cur_id = i;
                proc->state.s_balance = balance[i - 1];
                proc->state.s_balance_pending_in = 0;
                proc->history.s_history[0] = proc->state;
                proc->history.s_history_len = 1;
                proc->history.s_id = proc->cur_id;
                log_started(log, proc->cur_id);
            }
        }
    }
}

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
