
#include "log.h"

static const char *const log_pipe_open_fmt = "Pipe from process %d to %d was opened| %d %d\n";


void log_started(FILE *events_log_file, local_id process_id)
{
    printf(log_started_fmt, get_physical_time(), process_id, getpid(), getppid());
    fprintf(events_log_file, log_started_fmt, get_physical_time(), process_id, getpid(), getppid());
}

void log_received_all_started(FILE *events_log_file, local_id process_id)
{
    printf(log_received_all_started_fmt, get_physical_time(), process_id);
    fprintf(events_log_file, log_received_all_started_fmt, get_physical_time(), process_id);
}

void log_done(local_id process_id)
{
    FILE *events_log_file = fopen(events_log, "w+t");
    printf(log_done_fmt, get_physical_time(), process_id);
    fprintf(events_log_file, log_done_fmt, get_physical_time(), process_id);
}

void log_received_all_done(FILE *events_log_file, local_id process_id)
{
    printf(log_received_all_done_fmt, get_physical_time(), process_id);
    fprintf(events_log_file, log_received_all_done_fmt, get_physical_time(), process_id);
}

void log_pipe(FILE *pipes_log_file, local_id from, local_id to, int read, int write)
{
    fprintf(pipes_log_file, log_pipe_open_fmt, from, to, read, write);
}

void log_transfer_out(TransferOrder* trnsfr) 
{
    FILE *events_log_file = fopen(events_log, "w+t");
    printf(log_transfer_out_fmt, get_physical_time(), trnsfr->s_dst, trnsfr->s_amount, trnsfr->s_src);
    fprintf(events_log_file, log_transfer_out_fmt, get_physical_time(), trnsfr->s_dst, trnsfr->s_amount, trnsfr->s_src);
}

void log_transfer_in(TransferOrder* trnsfr) 
{
    FILE *events_log_file = fopen(events_log, "w+t");
    printf(log_transfer_in_fmt, get_physical_time(), trnsfr->s_dst, trnsfr->s_amount, trnsfr->s_src);
    fprintf(events_log_file, log_transfer_in_fmt, get_physical_time(), trnsfr->s_dst, trnsfr->s_amount, trnsfr->s_src);
}
