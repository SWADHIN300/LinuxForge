#ifndef STATS_H
#define STATS_H

#include "commit.h"

#define STATS_DIR "stats"

typedef struct {
    char   container_id[16];
    double cpu_percent;
    long   memory_usage;
    long   memory_limit;
    long   net_rx_bytes;
    long   net_tx_bytes;
    time_t timestamp;
} ContainerStats;

int stats_read_cpu(const char *container_id, double *cpu_percent);
int stats_read_memory(const char *container_id, long *usage, long *limit);
int stats_read_network(const char *container_id, long *rx_bytes, long *tx_bytes);
int stats_collect(const char *container_id, ContainerStats *stats);
int stats_save(const ContainerStats *stats);
int stats_history(const char *container_id, int minutes, int json_output);
int stats_print_live(const char *container_id);
int stats_cmd(int argc, char *argv[]);

#endif
