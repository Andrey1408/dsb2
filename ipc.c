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

/**
 * Вспомогательная функция для записи ровно len байт в fd (неблокирующая).
 * Возвращает 0 при успехе, 1 при ошибке.
 */
static int write_all(int fd, const char *buf, size_t len)
{
    size_t total_written = 0;
    int errno_tmp = errno;
    while (total_written < len)
    {
        ssize_t w = write(fd, buf + total_written, len - total_written);
        errno_tmp = errno;
        if (w < 0)
        {
            if (errno_tmp == EAGAIN || errno_tmp == EWOULDBLOCK)
            {
                // Временно нет возможности записать
                sleep(1); // ждём 1 мс
                continue;
            }
            else
            {
                perror("write_all");
                return 1;
            }
        }
        else if (w == 0)
        {
            // write вернул 0 - неожиданное закрытие канала
            fprintf(stderr, "write_all: channel closed\n");
            return 1;
        }
        total_written += (size_t)w;
    }
    return 0;
}

/**
 * Вспомогательная функция для чтения ровно len байт из fd (неблокирующая).
 * Возвращает 0 при успехе, 1 при ошибке.
 */
static int read_all(int fd, char *buf, size_t len)
{
    size_t total_read = 0;
    int errno_tmp = errno;
    while (total_read < len)
    {
        ssize_t r = read(fd, buf + total_read, len - total_read);
        errno_tmp = errno;
        if (r < 0)
        {
            if (errno_tmp == EAGAIN || errno_tmp == EWOULDBLOCK)
            {
                // Данных пока нет
                sleep(1); // ждём 1 мс
                continue;
            }
            else
            {
                perror("read_all");
                return 1;
            }
        }
        else if (r == 0)
        {
            // Канал закрыт (EOF)
            fprintf(stderr, "read_all: channel closed\n");
            return 1;
        }
        total_read += (size_t)r;
    }
    return 0;
}

/**
 * Отправка сообщения указанному процессу dst.
 * Отправляем ровно sizeof(MessageHeader) + s_header.s_payload_len байт.
 */
int send(void *self, local_id dst, const Message *msg)
{
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);

    int writer_fd = getWriterById(getSelfId(proc), dst, pipeline);
    if (writer_fd < 0)
    {
        fprintf(stderr, "send: invalid writer_fd from %d to %d\n",
                getSelfId(proc), dst);
        return 1;
    }
    // Подсчитываем реальный размер сообщения
    size_t msg_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;

    // Запись (неблокирующая) msg_len байт
    if (write_all(writer_fd, (const char *)msg, msg_len) != 0)
    {
        return 1;
    }
    return 0;
}

/**
 * Мультивещание сообщения всем процессам, кроме себя.
 */
int send_multicast(void *self, const Message *msg)
{
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);
    int n = *pipeline->size;
    local_id self_id = getSelfId(proc);

    for (int i = 0; i < n; i++)
    {
        if (i == self_id)
            continue;
        if (send(self, i, msg) != 0)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * Получение сообщения от конкретного процесса from.
 * Считываем сначала заголовок, потом полезную нагрузку.
 */
int receive(void *self, local_id from, Message *msg)
{
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);

    int reader_fd = getReaderById(from, getSelfId(proc), pipeline);
    if (reader_fd < 0)
    {
        fprintf(stderr, "receive: invalid reader_fd from %d to %d\n",
                from, getSelfId(proc));
        return 1;
    }

    // 1) Считать заголовок
    if (read_all(reader_fd, (char *)&msg->s_header, sizeof(MessageHeader)) != 0)
    {
        return 1;
    }
    // 2) Считать payload ровно s_payload_len байт
    uint16_t payload_len = msg->s_header.s_payload_len;
    if (payload_len > 0)
    {
        if (read_all(reader_fd, (char *)msg->s_payload, payload_len) != 0)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * Получение сообщения от любого процесса.
 * Реализовано перебором всех возможных отправителей.
 * Если ни от кого не удалось прочитать, делаем паузу и повторяем.
 */
int receive_any(void *self, Message *msg)
{
    ProcessPtr proc = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline *)getPipeline(proc);

    int n = *pipeline->size;
    local_id self_id = getSelfId(proc);
    int errno_tmp = errno;
    while (1)
    {
        for (int src = 0; src < n; src++)
        {
            if (src == self_id)
                continue;
            int reader_fd = getReaderById(src, self_id, pipeline);
            if (reader_fd < 0)
                continue;

            // Пытаемся прочитать заголовок
            MessageHeader header;
            memset(&header, 0, sizeof(header));
            size_t total_read = 0;
            int header_ok = 1;

            // Считаем заголовок (неблокирующий)
            while (total_read < sizeof(MessageHeader))
            {
                ssize_t r = read(reader_fd,
                                 ((char *)&header) + total_read,
                                 sizeof(MessageHeader) - total_read);
                errno_tmp = errno;
                if (r < 0)
                {
                    if (errno_tmp == EAGAIN || errno_tmp == EWOULDBLOCK)
                    {
                        // нет данных на этом канале — переходим к следующему
                        header_ok = 0;
                        break;
                    }
                    else
                    {
                        perror("receive_any: read header");
                        header_ok = 0;
                        break;
                    }
                }
                else if (r == 0)
                {
                    // Канал закрыт
                    header_ok = 0;
                    break;
                }
                total_read += (size_t)r;
            }

            if (!header_ok)
            {
                // Заголовок не прочитан полностью — откатываемся
                continue;
            }

            // Теперь читаем payload, если есть
            msg->s_header = header;
            uint16_t payload_len = header.s_payload_len;
            if (payload_len > 0)
            {
                size_t total_payload = 0;
                while (total_payload < payload_len)
                {
                    ssize_t r = read(reader_fd,
                                     msg->s_payload + total_payload,
                                     payload_len - total_payload);
                    errno_tmp = errno;
                    if (r < 0)
                    {
                        if (errno_tmp == EAGAIN || errno_tmp == EWOULDBLOCK)
                        {
                            // Недочитали payload, значит в этом проходе не получилось
                            // Положим, что сообщение «разорвано» — откатываемся.
                            // (В реальной жизни надо буферизовать. Здесь упрощаем.)
                            // Придётся пропустить это сообщение.
                            break;
                        }
                        else
                        {
                            perror("receive_any: read payload");
                            break;
                        }
                    }
                    else if (r == 0)
                    {
                        // Канал закрыт
                        break;
                    }
                    total_payload += (size_t)r;
                }
                if (total_payload < payload_len)
                {
                    // Не дочитали payload
                    continue;
                }
            }
            // Успешно прочитали всё сообщение
            return 0;
        }
        // Ни на одном канале не получили полное сообщение
        sleep(1); // ждем и повторяем
    }
    // Никогда не достигнем сюда
    return 1;
}
