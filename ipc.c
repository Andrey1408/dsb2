#include "ipc.h"
#include "pipe_utils.h"
#include "process_factory.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>


// Функция для записи всех байтов в пайп (неблокирующая)
static int write_all(int fd, const char *buf, size_t len) {
    size_t total_written = 0;
    while (total_written < len) {
        ssize_t w = write(fd, buf + total_written, len - total_written);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sleep_1ms();
                continue;
            } else {
                perror("[write_all] Ошибка записи");
                return 1;
            }
        } else if (w == 0) {
            fprintf(stderr, "[write_all] Канал закрыт\n");
            return 1;
        }
        total_written += (size_t)w;
    }
    return 0;
}

// Функция для чтения всех байтов из пайпа (неблокирующая)
static int read_all(int fd, char *buf, size_t len) {
    size_t total_read = 0;
    while (total_read < len) {
        ssize_t r = read(fd, buf + total_read, len - total_read);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sleep_1ms();
                continue;
            } else {
                perror("[read_all] Ошибка чтения");
                return 1;
            }
        } else if (r == 0) {
            fprintf(stderr, "[read_all] Канал закрыт\n");
            return 1;
        }
        total_read += (size_t)r;
    }
    return 0;
}

// Отправка сообщения
int send(void *self, local_id dst, const Message *msg) {
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);
    int writer_fd = getWriterById(getSelfId(proc), dst, pipeline);

    if (writer_fd < 0) {
        fprintf(stderr, "[send] Ошибка: неверный FD %d -> %d\n", getSelfId(proc), dst);
        return 1;
    }

    printf("[send] %d -> %d | type: %d | len: %d\n",
           getSelfId(proc), dst, msg->s_header.s_type, msg->s_header.s_payload_len);
    fflush(stdout);

    size_t msg_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;
    return write_all(writer_fd, (const char *)msg, msg_len);
}

// Мультивещание сообщения
int send_multicast(void *self, const Message *msg) {
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);
    int n = *pipeline->size;
    local_id self_id = getSelfId(proc);

    printf("[send_multicast] Процесс %d отправляет сообщение всем\n", self_id);
    fflush(stdout);

    for (int i = 0; i < n; i++) {
        if (i == self_id) continue;
        if (send(self, i, msg) != 0) {
            return 1;
        }
    }
    return 0;
}

// Получение сообщения от конкретного процесса
int receive(void *self, local_id from, Message *msg) {
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);
    int reader_fd = getReaderById(from, getSelfId(proc), pipeline);

    if (reader_fd < 0) {
        fprintf(stderr, "[receive] Ошибка: неверный FD %d <- %d\n",
                getSelfId(proc), from);
        return 1;
    }

    if (read_all(reader_fd, (char *)&msg->s_header, sizeof(MessageHeader)) != 0) {
        return 1;
    }

    uint16_t payload_len = msg->s_header.s_payload_len;
    if (payload_len > 0) {
        if (read_all(reader_fd, (char *)msg->s_payload, payload_len) != 0) {
            return 1;
        }
    }

    printf("[receive] %d <- %d | type: %d | len: %d\n",
           getSelfId(proc), from, msg->s_header.s_type, payload_len);
    fflush(stdout);

    return 0;
}

// Получение сообщения от любого процесса
int receive_any(void *self, Message *msg) {
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);
    int n = *pipeline->size;
    local_id self_id = getSelfId(proc);
    time_t start_time = time(NULL);

    printf("[receive_any] %d ждёт сообщение...\n", self_id);
    fflush(stdout);

    while (1) {
        for (int src = 0; src < n; src++) {
            if (src == self_id) continue;
            int reader_fd = getReaderById(src, self_id, pipeline);
            if (reader_fd < 0) continue;

            MessageHeader header;
            ssize_t r = read(reader_fd, &header, sizeof(MessageHeader));
            if (r == sizeof(MessageHeader)) {
                msg->s_header = header;
                if (header.s_payload_len > 0) {
                    read(reader_fd, msg->s_payload, header.s_payload_len);
                }

                printf("[receive_any] %d <- %d | type: %d | len: %d\n",
                       self_id, src, header.s_type, header.s_payload_len);
                fflush(stdout);
                return 0;
            }
        }

        if (time(NULL) - start_time > 3) {
            printf("[receive_any] %d Таймаут ожидания!\n", self_id);
            return 1;
        }
        sleep(1);
    }
}
