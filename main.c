#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "banking.h"
#include "pipe_utils.h"
#include "process_factory.h"

timestamp_t get_physical_time();
void print_history(const AllHistory *history);

typedef struct {
    balance_t balance;
    BalanceHistory history;
} Account;

void update_history(Account *account) {
    timestamp_t t = get_physical_time();
    if (account->history.s_history_len < MAX_T) {
        BalanceState *state = &account->history.s_history[account->history.s_history_len];
        state->s_time = t;
        state->s_balance = account->balance;
        state->s_balance_pending_in = 0;
        account->history.s_history_len++;
    }
}

//---------------------------------------------------------------------
// Функция transfer() вызывается процессом «К» (родительским) при выполнении bank_robbery().
// Передаётся parent_data (структура Process), src – id процесса-отправителя, dst – id получателя, amount – сумма перевода.
void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    ProcessPtr proc = (ProcessPtr)parent_data;

    TransferOrder order = {src, dst, amount};
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_payload_len = sizeof(TransferOrder);
    msg.s_header.s_local_time = get_physical_time();
    memcpy(msg.s_payload, &order, sizeof(TransferOrder));

    printf("[Parent] Отправляем TRANSFER %d -> %d: $%d\n", src, dst, amount);
    fflush(stdout);

    if (send(proc, src, &msg) != 0) {
        fprintf(stderr, "[Parent] Ошибка отправки TRANSFER в %d\n", src);
    }

    Message ack;
    if (receive(proc, dst, &ack) != 0) {
        fprintf(stderr, "[Parent] Ошибка получения ACK от %d\n", dst);
    }
}

//---------------------------------------------------------------------
// Основная функция лабораторной работы (файл pa23.c).
// Командная строка: ./pa2 -p <child_count> <balance1> <balance2> ... <balanceN>
int main(int argc, char *argv[]) {
    int child_count = 0;
    int arg_index = 1;
    if (strcmp(argv[arg_index], "-p") == 0) {
        if (arg_index + 1 < argc) {
            child_count = atoi(argv[arg_index + 1]);
            arg_index += 2;
        }
    }

    int process_count = child_count + 1;
    FILE *events_log_file = fopen(events_log, "w");

    Pipeline *pipeline = create_pipeline(process_count);
    pid_t pids[process_count];
    pids[0] = getpid();

    // Форкаем дочерние процессы
    for (int i = 1; i < process_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {  // Дочерний процесс
            local_id my_id = i;
            close_unused_pipes(pipeline, my_id);
            ProcessPtr proc = createProcess(my_id, process_count, pipeline);

            Account account;
            account.balance = (balance_t)atoi(argv[arg_index + i - 1]);
            account.history.s_id = my_id;
            account.history.s_history_len = 0;
            update_history(&account);

            printf("[Child %d] STARTED с балансом: $%d\n", my_id, account.balance);
            fflush(stdout);

            Message msg;
            msg.s_header.s_magic = MESSAGE_MAGIC;
            msg.s_header.s_type = STARTED;
            msg.s_header.s_payload_len = 0;
            msg.s_header.s_local_time = get_physical_time();
            send_multicast(proc, &msg);

            for (local_id src = 1; src < process_count; src++) {
                if (src == my_id) continue;
                Message rmsg;
                if (receive(proc, src, &rmsg) != 0) {
                    fprintf(stderr, "[Child %d] Ошибка получения STARTED от %d\n", my_id, src);
                }
            }
            printf("[Child %d] Получил все STARTED\n", my_id);
            fflush(stdout);

            // Ожидание сообщений и обработка транзакций
            int stop_received = 0;
            while (!stop_received) {
                Message rmsg;
                if (receive_any(proc, &rmsg) == 0) {
                    if (rmsg.s_header.s_type == TRANSFER) {
                        TransferOrder order;
                        memcpy(&order, rmsg.s_payload, sizeof(TransferOrder));

                        if (my_id == order.s_src) {
                            account.balance -= order.s_amount;
                            printf("[Child %d] Отправил $%d -> %d\n", my_id, order.s_amount, order.s_dst);
                            send(proc, order.s_dst, &rmsg);
                        } else if (my_id == order.s_dst) {
                            account.balance += order.s_amount;
                            printf("[Child %d] Получил $%d от %d\n", my_id, order.s_amount, order.s_src);
                            Message ack;
                            ack.s_header.s_magic = MESSAGE_MAGIC;
                            ack.s_header.s_type = ACK;
                            ack.s_header.s_payload_len = 0;
                            ack.s_header.s_local_time = get_physical_time();
                            send(proc, 0, &ack);
                        }
                        update_history(&account);
                    } else if (rmsg.s_header.s_type == STOP) {
                        stop_received = 1;
                    }
                }
            }

            printf("[Child %d] Отправляет DONE, баланс: $%d\n", my_id, account.balance);
            fflush(stdout);

            Message done;
            done.s_header.s_magic = MESSAGE_MAGIC;
            done.s_header.s_type = DONE;
            done.s_header.s_payload_len = 0;
            done.s_header.s_local_time = get_physical_time();
            send_multicast(proc, &done);

            Message hist_msg;
            hist_msg.s_header.s_magic = MESSAGE_MAGIC;
            hist_msg.s_header.s_type = BALANCE_HISTORY;
            hist_msg.s_header.s_payload_len = sizeof(BalanceHistory);
            memcpy(hist_msg.s_payload, &account.history, sizeof(BalanceHistory));
            hist_msg.s_header.s_local_time = get_physical_time();
            send(proc, 0, &hist_msg);

            exit(0);
        } else {
            pids[i] = pid;
        }
    }

    local_id my_id = 0;
    close_unused_pipes(pipeline, my_id);
    ProcessPtr proc = createProcess(my_id, process_count, pipeline);

    for (local_id src = 1; src < process_count; src++) {
        Message rmsg;
        if (receive(proc, src, &rmsg) != 0) {
            fprintf(stderr, "[Parent] Ошибка получения STARTED от %d\n", src);
        }
    }
    printf("[Parent] Получены все STARTED\n");
    fflush(stdout);

    bank_robbery(proc, process_count - 1);

    printf("[Parent] Отправляем STOP\n");
    fflush(stdout);

    Message stop_msg;
    stop_msg.s_header.s_magic = MESSAGE_MAGIC;
    stop_msg.s_header.s_type = STOP;
    stop_msg.s_header.s_payload_len = 0;
    stop_msg.s_header.s_local_time = get_physical_time();
    send_multicast(proc, &stop_msg);

    for (local_id src = 1; src < process_count; src++) {
        Message rmsg;
        if (receive(proc, src, &rmsg) != 0) {
            fprintf(stderr, "[Parent] Ошибка получения DONE от %d\n", src);
        }
    }
    printf("[Parent] Получены все DONE\n");
    fflush(stdout);

    AllHistory all_history;
    all_history.s_history_len = child_count;
    for (local_id src = 1; src < process_count; src++) {
        Message rmsg;
        if (receive(proc, src, &rmsg) != 0) {
            fprintf(stderr, "[Parent] Ошибка получения BALANCE_HISTORY от %d\n", src);
        } else {
            memcpy(&all_history.s_history[src], rmsg.s_payload, sizeof(BalanceHistory));
        }
    }
    print_history(&all_history);

    for (int i = 1; i < process_count; i++) {
        wait(NULL);
    }

    fclose(events_log_file);
    free(proc);
    free(pipeline);
    return 0;
}
