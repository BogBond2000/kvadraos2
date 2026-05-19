#include "../common/SharedStruct.h"
#include "../common/SharedMemory.h"
#include "StatsCollector.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <cstring>

static volatile bool running = true;
void signalHandler(int) { running = false; }

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const size_t shm_size = sizeof(SharedStats);
    SharedMemory shm(SHM_NAME, shm_size, true);
    SharedStats* stats = reinterpret_cast<SharedStats*>(shm.get());
    initSharedStats(stats);

    StatsCollector collector;
    while (running) {
        pthread_mutex_lock(&stats->mutex);
        collector.update(stats);
        pthread_mutex_unlock(&stats->mutex);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    shm.unlink();
    return 0;
}