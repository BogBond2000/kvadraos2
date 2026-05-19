#pragma once

#include <cstdint>
#include <cstring>
#include <pthread.h>

#define SHM_NAME "/sys_monitor_shm"
#define MAX_CPUS 64
#define MAX_TOP_PROCS 20
#define PROC_NAME_LEN 256

struct ProcInfo {
    int pid;
    char name[PROC_NAME_LEN];
    double cpu_usage;
    uint64_t memory_rss_kb;
};

struct SharedStats {
    pthread_mutex_t mutex;
    uint64_t timestamp;

    int num_cpus;
    double cpu_usage_total;
    double cpu_usage_per_core[MAX_CPUS];

    uint64_t mem_total_kb;
    uint64_t mem_used_kb;
    uint64_t mem_free_kb;
    uint64_t mem_available_kb;

    uint64_t swap_total_kb;
    uint64_t swap_used_kb;

    double load_avg_1;
    double load_avg_5;
    double load_avg_15;

    int num_top_procs;
    ProcInfo top_procs[MAX_TOP_PROCS];
};

inline void initSharedStats(SharedStats* stats) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&stats->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    std::memset(stats, 0, sizeof(SharedStats));
}