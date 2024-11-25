#include "message_logic.h"

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

    Message msg_out;
    msg_out.s_header.s_type = STARTED;

    // Waiting for ACK from dst
    while (msg_out.s_header.s_type != ACK)
    {
        receive(parent_data, dst, &msg_out);
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

void transfer_process(pipe_ut *pp, Message *msg, FILE* events_log_file)
{
    TransferOrder *trnsfr = (TransferOrder *)msg->s_payload;
    if (pp->cur_id == trnsfr->s_src)
    {
        pp->state.s_balance += trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        send(pp, trnsfr->s_dst, msg);
        log_transfer_out(trnsfr, events_log_file);
    }
    else
    {
        pp->state.s_balance -= trnsfr->s_amount;
        balance_history(&(pp->history), pp->state);
        Message msg = create_message(ACK, NULL, 0);
        send(pp, PARENT_ID, &msg);
        log_transfer_in(trnsfr, events_log_file);
        free(trnsfr);
    }
}
