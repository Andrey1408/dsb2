#ifndef CUSTOM_LOG_DSB2
#define CUSTOM_LOG_DSB2

#include "pa2345.h"
#include "common.h"
#include "ipc.h"
#include "banking.h"
#include "pipe_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void log_started(FILE *events_log_file, local_id process_id);

void log_received_all_started(FILE *events_log_file, local_id process_id);

void log_done(local_id process_id);

void log_received_all_done(FILE *events_log_file, local_id process_id);

void log_pipe(FILE *pipes_log_file, local_id from, local_id to, int read, int write);

void log_transfer_out( TransferOrder* trnsfr );

void log_transfer_in( TransferOrder* trnsfr );

#endif
