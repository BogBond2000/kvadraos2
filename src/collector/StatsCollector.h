#pragma once

#include "../common/SharedStruct.h"
#include <vector>
#include <map>
#include <string>

struct ProcPrevStats {
    unsigned long long utime;
    unsigned long long stime;
    unsigned long long cutime;
    unsigned long long cstime;
    unsigned long long starttime;
    unsigned long long total_time; // cached
};

class StatsCollector {
public:
    StatsCollector();
    ~StatsCollector();

    void update(SharedStats* stats);

private:
    void readCpuStats(SharedStats* stats);
    void readMemStats(SharedStats* stats);
    void readLoadAvg(SharedStats* stats);
    void readProcessStats(SharedStats* stats);

    // CPU prev values
    struct CpuPrev {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    };
    std::vector<CpuPrev> prevCpuStats_;
    unsigned long long prevTotalCpu_;

    // Process prev values
    std::map<int, ProcPrevStats> prevProcStats_;
    unsigned long long prevTotalCpuTime_;

    // helpers
    unsigned long long getTotalCpuTime(const std::vector<unsigned long long>& cur);
    double calculateCpuUsage(const CpuPrev& prev, const CpuPrev& cur);
};