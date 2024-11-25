#include "pa23.h"

void child_stopping(pipe_ut *pp, const int *processes_left_counter, FILE *events_log_file)
{
    printf("In child %d, in stop\n", pp->cur_id);
    int i = *processes_left_counter;
    for (int id = 1; i < pp->size - 2 || id < pp->size - 1; id++)
    {
        Message msg;
        if (id != pp->cur_id)
        {
            int status = receive(pp, id, &msg);
            if (status == 0)
            {
                switch (msg.s_header.s_type)
                {
                case DONE:
                    printf("case done %d\n", pp->cur_id);
                    i++;
                    continue;
                case TRANSFER:
                    printf("case transfer %d\n", pp->cur_id);
                    pp->state.s_time = get_physical_time();
                    transfer_process(pp, &msg, events_log_file);
                    id = 0;
                    continue;
                default:
                    printf("default broke in child stop %d\n", pp->cur_id);
                    continue;
                }
            }
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
    exit(EXIT_SUCCESS);
}

void child_work(pipe_ut *pp, FILE *events_log_file)
{
    Message msg = create_message(STARTED, NULL, 0);
    send(pp, PARENT_ID, &msg);
    int i = 0;
    while (i < pp->size - 2)
    {
        receive_any(pp, &msg);
        printf("child %d received: %d \n", pp->cur_id, msg.s_header.s_type);
        switch (msg.s_header.s_type)
        {
        case TRANSFER:
            printf("child transfer %d\n", pp->cur_id);
            pp->state.s_time = get_physical_time();
            transfer_process(pp, &msg, events_log_file);
            continue;
        case STOP:
            printf("child stopped %d\n", pp->cur_id);
            child_stopping(pp, &i, events_log_file);
            break;
        case DONE:
            i++;
            continue;
        default:
            printf("default broke in child work %d\n", pp->cur_id);
            continue;
        }
    }
    exit(EXIT_FAILURE);
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
