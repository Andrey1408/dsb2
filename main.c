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
void print_history(const AllHistory * history);


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
void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
    ProcessPtr proc = (ProcessPtr)parent_data;

    // Сформировать структуру TransferOrder
    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;

    // Сформировать сообщение типа TRANSFER с полезной нагрузкой TransferOrder
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_payload_len = sizeof(TransferOrder);
    memcpy(msg.s_payload, &order, sizeof(TransferOrder));
    msg.s_header.s_local_time = get_physical_time();

    // Отправить сообщение процессу Csrc
    if (send(proc, src, &msg) != 0) {
        fprintf(stderr, "Parent: failed to send TRANSFER to process %d\n", src);
    }

    // Ожидать подтверждения (ACK) от процесса Cdst
    Message ack;
    if (receive(proc, dst, &ack) != 0) {
        fprintf(stderr, "Parent: failed to receive ACK from process %d\n", dst);
    }
    // (Можно добавить проверку типа ack.s_header.s_type)
}

//---------------------------------------------------------------------
// Основная функция лабораторной работы (файл pa23.c).
// Командная строка: ./pa2 -p <child_count> <balance1> <balance2> ... <balanceN>
int main(int argc, char * argv[]) {
    int child_count = 0;
    int arg_index = 1;
    if (strcmp(argv[arg_index], "-p") == 0) {
        if (arg_index + 1 < argc) {
            child_count = atoi(argv[arg_index + 1]);
            arg_index += 2;
        }
    }


    int process_count = child_count + 1; // Родитель (K) + дочерние процессы (С)

    // Открыть events.log до форка (требование проверки)
    FILE *events_log_file = fopen(events_log, "a");

    // Создать полносвязную топологию каналов
    Pipeline *pipeline = create_pipeline(process_count);

    pid_t pids[process_count];
    pids[0] = getpid();  // Родительский процесс (K)

    // Форкаем дочерние процессы (процессы типа "С")
    for (int i = 1; i < process_count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {  // Дочерний процесс
            local_id my_id = i;
            close_unused_pipes(pipeline, my_id);
            ProcessPtr proc = createProcess(my_id, process_count, pipeline);

            // Инициализировать счёт (баланс) дочернего процесса из параметров командной строки.
            Account account;
            account.balance = (balance_t)atoi(argv[arg_index + i - 1]);
            account.history.s_id = my_id;
            account.history.s_history_len = 0;
            update_history(&account);  // Записать начальное состояние

            // Логирование события STARTED (используем формат log_started_fmt из pa2345.h)
            char started_msg[256];
            snprintf(started_msg, sizeof(started_msg), log_started_fmt,
                     get_physical_time(), my_id, getpid(), getppid(), account.balance);
            printf("%s", started_msg);
            fprintf(events_log_file, "%s", started_msg);
            fflush(events_log_file);

            // Отправить сообщение STARTED (рассылка всем)
            Message msg;
            memset(&msg, 0, sizeof(msg));
            msg.s_header.s_magic = MESSAGE_MAGIC;
            msg.s_header.s_type = STARTED;
            msg.s_header.s_payload_len = strlen(started_msg);
            strncpy(msg.s_payload, started_msg, sizeof(msg.s_payload));
            msg.s_header.s_local_time = get_physical_time();
            send_multicast(proc, &msg);

            // Ожидание сообщений STARTED от всех остальных дочерних процессов
            for (local_id src = 1; src < process_count; src++) {
                if (src == my_id)
                    continue;
                Message rmsg;
                if (receive(proc, src, &rmsg) != 0) {
                    fprintf(stderr, "Child %d: error receiving STARTED from %d\n", my_id, src);
                }
            }
            char all_started_msg[256];
            snprintf(all_started_msg, sizeof(all_started_msg), log_received_all_started_fmt,
                     get_physical_time(), my_id);
            printf("%s", all_started_msg);
            fprintf(events_log_file, "%s", all_started_msg);
            fflush(events_log_file);

            // Главный цикл обработки сообщений (асинхронный обмен)
            int stop_received = 0;
            while (!stop_received) {
                Message rmsg;
                // Проходим по всем каналам (можно использовать блокирующее получение)
                for (local_id src = 0; src < process_count; src++) {
                    if (src == my_id)
                        continue;
                    if (receive(proc, src, &rmsg) == 0) {
                        if (rmsg.s_header.s_type == TRANSFER) {
                            // Обработка сообщения TRANSFER. Полезная нагрузка — структура TransferOrder.
                            TransferOrder order;
                            memcpy(&order, rmsg.s_payload, sizeof(TransferOrder));
                            if (my_id == order.s_src) {
                                // Если я отправитель (Csrc): списываю сумму и пересылаю сообщение получателю.
                                account.balance -= order.s_amount;
                                char transfer_out_msg[256];
                                snprintf(transfer_out_msg, sizeof(transfer_out_msg), log_transfer_out_fmt,
                                         get_physical_time(), my_id, order.s_amount, order.s_dst);
                                printf("%s", transfer_out_msg);
                                fprintf(events_log_file, "%s", transfer_out_msg);
                                fflush(events_log_file);
                                if (send(proc, order.s_dst, &rmsg) != 0) {
                                    fprintf(stderr, "Child %d: error forwarding TRANSFER to %d\n",
                                            my_id, order.s_dst);
                                }
                            } else if (my_id == order.s_dst) {
                                // Если я получатель (Cdst): увеличиваю баланс и отправляю ACK родителю.
                                account.balance += order.s_amount;
                                char transfer_in_msg[256];
                                snprintf(transfer_in_msg, sizeof(transfer_in_msg), log_transfer_in_fmt,
                                         get_physical_time(), my_id, order.s_amount, order.s_src);
                                printf("%s", transfer_in_msg);
                                fprintf(events_log_file, "%s", transfer_in_msg);
                                fflush(events_log_file);
                                Message ack;
                                memset(&ack, 0, sizeof(ack));
                                ack.s_header.s_magic = MESSAGE_MAGIC;
                                ack.s_header.s_type = ACK;
                                ack.s_header.s_payload_len = 0;
                                ack.s_header.s_local_time = get_physical_time();
                                if (send(proc, 0, &ack) != 0) {
                                    fprintf(stderr, "Child %d: error sending ACK to parent\n", my_id);
                                }
                            }
                            update_history(&account);
                        } else if (rmsg.s_header.s_type == STOP) {
                            stop_received = 1;
                            break;
                        }
                        // Остальные типы сообщений игнорируем.
                    }
                }
            } // end while

            // После получения STOP: логируем DONE
            char done_msg[256];
            snprintf(done_msg, sizeof(done_msg), log_done_fmt,
                     get_physical_time(), my_id, account.balance);
            printf("%s", done_msg);
            fprintf(events_log_file, "%s", done_msg);
            fflush(events_log_file);
            Message done;
            memset(&done, 0, sizeof(done));
            done.s_header.s_magic = MESSAGE_MAGIC;
            done.s_header.s_type = DONE;
            done.s_header.s_payload_len = strlen(done_msg);
            strncpy(done.s_payload, done_msg, sizeof(done.s_payload));
            done.s_header.s_local_time = get_physical_time();
            send_multicast(proc, &done);

            // Отправить родителю сообщение BALANCE_HISTORY с историей баланса.
            Message hist_msg;
            memset(&hist_msg, 0, sizeof(hist_msg));
            hist_msg.s_header.s_magic = MESSAGE_MAGIC;
            hist_msg.s_header.s_type = BALANCE_HISTORY;
            hist_msg.s_header.s_payload_len = sizeof(BalanceHistory);
            memcpy(hist_msg.s_payload, &account.history, sizeof(BalanceHistory));
            hist_msg.s_header.s_local_time = get_physical_time();
            if (send(proc, 0, &hist_msg) != 0) {
                fprintf(stderr, "Child %d: error sending BALANCE_HISTORY to parent\n", my_id);
            }
            exit(0);
        } else {
            pids[i] = pid;
        }
    } // end for (fork)

    // --- Код родительского процесса (процесс "К") ---
    local_id my_id = 0;
    close_unused_pipes(pipeline, my_id);
    ProcessPtr proc = createProcess(my_id, process_count, pipeline);

    // Родительский процесс не имеет счета, но логирует своё STARTED-событие.
    char parent_started[256];
    snprintf(parent_started, sizeof(parent_started), "Parent process (id %d) STARTED\n", my_id);
    printf("%s", parent_started);
    fprintf(events_log_file, "%s", parent_started);
    fflush(events_log_file);

    // Родитель ждёт сообщения STARTED от всех дочерних процессов.
    for (local_id src = 1; src < process_count; src++) {
        Message rmsg;
        if (receive(proc, src, &rmsg) != 0) {
            fprintf(stderr, "Parent: error receiving STARTED from %d\n", src);
        }
    }

    // Вызвать функцию bank_robbery(), которая выполняет ряд переводов.
    // Функция bank_robbery() реализована в bank_robbery.c и вызывает transfer().
    bank_robbery(proc, process_count - 1);  // число дочерних процессов

    // После bank_robbery() родитель отправляет сообщение STOP всем дочерним процессам.
    Message stop_msg;
    memset(&stop_msg, 0, sizeof(stop_msg));
    stop_msg.s_header.s_magic = MESSAGE_MAGIC;
    stop_msg.s_header.s_type = STOP;
    stop_msg.s_header.s_payload_len = 0;
    stop_msg.s_header.s_local_time = get_physical_time();
    for (local_id dst = 1; dst < process_count; dst++) {
        if (send(proc, dst, &stop_msg) != 0) {
            fprintf(stderr, "Parent: error sending STOP to %d\n", dst);
        }
    }

    // Родитель ждёт сообщения DONE от всех дочерних процессов.
    for (local_id src = 1; src < process_count; src++) {
        Message rmsg;
        if (receive(proc, src, &rmsg) != 0) {
            fprintf(stderr, "Parent: error receiving DONE from %d\n", src);
        }
    }
    printf("Parent: received all DONE messages\n");
    fprintf(events_log_file, "Parent: received all DONE messages\n");
    fflush(events_log_file);

    // Родитель получает BALANCE_HISTORY от каждого дочернего процесса и агрегирует их.
    AllHistory all_history;
    all_history.s_history_len = child_count;
    for (local_id src = 1; src < process_count; src++) {
        Message rmsg;
        if (receive(proc, src, &rmsg) != 0) {
            fprintf(stderr, "Parent: error receiving BALANCE_HISTORY from %d\n", src);
        } else {
            memcpy(&all_history.s_history[src], rmsg.s_payload, sizeof(BalanceHistory));
        }
    }
    // Вывести агрегированную историю балансов
    print_history(&all_history);

    // Ждать завершения всех дочерних процессов
    for (int i = 1; i < process_count; i++) {
        wait(NULL);
    }

    fclose(events_log_file);
    free(proc);
    free(pipeline);

    return 0;
}
