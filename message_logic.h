#ifndef MESSAGE_LOGIC_DSB
#define MESSAGE_LOGIC_DSB

#include "ipc.h"
#include "banking.h"
#include "pipe_utils.h"
#include "log.h"

Message create_message(MessageType type, void* contents, uint16_t size);
void transfer(void* parent_data, local_id src, local_id dst, balance_t amount);
void balance_history(BalanceHistory* history, BalanceState state);
void transfer_process(pipe_ut *pp, Message *msg, FILE* events_log_file);
void wait_messages(pipe_ut *pp, MessageType status);

#endif
