#include "StatsCollector.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <cstring>
#include <unistd.h>

StatsCollector::StatsCollector() {
    prevTotalCpuTime_ = 0;
}

StatsCollector::~StatsCollector() {}

void StatsCollector::update(SharedStats* stats) {
    readCpuStats(stats);
    readMemStats(stats);
    readLoadAvg(stats);
    readProcessStats(stats);
    stats->timestamp = time(nullptr);
}

void StatsCollector::readCpuStats(SharedStats* stats) {
    std::ifstream proc_stat("/proc/stat");
    std::string line;

    // Read total CPU line: "cpu  ..."
    std::vector<unsigned long long> total_vals;
    if (std::getline(proc_stat, line) && line.substr(0,4) == "cpu ") {
        std::istringstream iss(line.substr(5));
        unsigned long long val;
        while (iss >> val) total_vals.push_back(val);
    }

    // Read per-core lines
    std::vector<CpuPrev> curCpuStats;
    curCpuStats.reserve(MAX_CPUS);
    std::vector<double> per_core_usage;

    while (std::getline(proc_stat, line)) {
        if (line.substr(0,3) == "cpu" && line[3] != ' ') {
            // line like "cpu0 ..."
            std::istringstream iss(line.substr(4));
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            curCpuStats.push_back({user, nice, system, idle, iowait, irq, softirq, steal});
        } else if (line.substr(0,4) != "cpu") {
            break;
        }
    }

    stats->num_cpus = curCpuStats.size();
    if (stats->num_cpus > MAX_CPUS) stats->num_cpus = MAX_CPUS;

    // Calculate per-core usage if previous exists
    if (prevCpuStats_.size() == curCpuStats.size()) {
        for (size_t i = 0; i < curCpuStats.size() && i < MAX_CPUS; ++i) {
            double usage = calculateCpuUsage(prevCpuStats_[i], curCpuStats[i]);
            stats->cpu_usage_per_core[i] = usage;
        }
    } else {
        for (int i = 0; i < stats->num_cpus; ++i) stats->cpu_usage_per_core[i] = 0.0;
    }

    // Calculate total CPU usage from the first line "cpu"
    if (total_vals.size() >= 4) {
        unsigned long long user = total_vals[0];
        unsigned long long nice = total_vals[1];
        unsigned long long system = total_vals[2];
        unsigned long long idle = total_vals[3];
        unsigned long long iowait = total_vals.size() > 4 ? total_vals[4] : 0;
        unsigned long long irq = total_vals.size() > 5 ? total_vals[5] : 0;
        unsigned long long softirq = total_vals.size() > 6 ? total_vals[6] : 0;
        unsigned long long steal = total_vals.size() > 7 ? total_vals[7] : 0;

        unsigned long long total_idle = idle + iowait;
        unsigned long long total_non_idle = user + nice + system + irq + softirq + steal;
        unsigned long long total = total_idle + total_non_idle;

        static unsigned long long prev_total = 0;
        static unsigned long long prev_idle = 0;
        if (prev_total != 0) {
            unsigned long long diff_total = total - prev_total;
            unsigned long long diff_idle = total_idle - prev_idle;
            stats->cpu_usage_total = 100.0 * (diff_total - diff_idle) / diff_total;
        } else {
            stats->cpu_usage_total = 0.0;
        }
        prev_total = total;
        prev_idle = total_idle;
    }

    prevCpuStats_ = std::move(curCpuStats);
}

void StatsCollector::readMemStats(SharedStats* stats) {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.compare(0, 8, "MemTotal") == 0) {
            sscanf(line.c_str(), "MemTotal: %lu kB", &stats->mem_total_kb);
        } else if (line.compare(0, 9, "MemFree") == 0) {
            sscanf(line.c_str(), "MemFree: %lu kB", &stats->mem_free_kb);
        } else if (line.compare(0, 13, "MemAvailable") == 0) {
            sscanf(line.c_str(), "MemAvailable: %lu kB", &stats->mem_available_kb);
        } else if (line.compare(0, 8, "SwapTotal") == 0) {
            sscanf(line.c_str(), "SwapTotal: %lu kB", &stats->swap_total_kb);
        } else if (line.compare(0, 7, "SwapFree") == 0) {
            unsigned long long swap_free;
            sscanf(line.c_str(), "SwapFree: %lu kB", &swap_free);
            stats->swap_used_kb = stats->swap_total_kb - swap_free;
        }
    }
    stats->mem_used_kb = stats->mem_total_kb - stats->mem_free_kb;
}

void StatsCollector::readLoadAvg(SharedStats* stats) {
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg) {
        loadavg >> stats->load_avg_1 >> stats->load_avg_5 >> stats->load_avg_15;
    }
}

void StatsCollector::readProcessStats(SharedStats* stats) {
    // Get total CPU time from /proc/stat
    unsigned long long total_cpu_time = 0;
    {
        std::ifstream proc_stat("/proc/stat");
        std::string line;
        if (std::getline(proc_stat, line) && line.substr(0,4) == "cpu ") {
            std::istringstream iss(line.substr(5));
            unsigned long long val;
            while (iss >> val) total_cpu_time += val;
        }
    }

    std::map<int, ProcPrevStats> current_proc_stats;
    std::vector<std::pair<int, double>> proc_cpu_usage; // pid, usage

    DIR* dir = opendir("/proc");
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        int pid = atoi(entry->d_name);
        if (pid == 0) continue;

        std::string stat_path = "/proc/" + std::string(entry->d_name) + "/stat";
        std::ifstream stat_file(stat_path);
        if (!stat_file) continue;

        std::string line;
        if (!std::getline(stat_file, line)) continue;

        // Parse stat
        size_t last_paren = line.rfind(')');
        if (last_paren == std::string::npos) continue;
        size_t first_paren = line.find('(');
        if (first_paren == std::string::npos) continue;
        std::string comm = line.substr(first_paren+1, last_paren - first_paren - 1);
        std::istringstream iss(line.substr(last_paren+2));
        char state;
        unsigned long long ppid, pgrp, session, tty_nr, tpgid, flags, minflt, cminflt, majflt, cmajflt;
        unsigned long long utime, stime, cutime, cstime, priority, nice, num_threads, itrealvalue, starttime, vsize, rss;
        iss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
            >> utime >> stime >> cutime >> cstime >> priority >> nice >> num_threads >> itrealvalue >> starttime >> vsize >> rss;

        unsigned long long total_time = utime + stime + cutime + cstime;
        ProcPrevStats prev;
        bool has_prev = (prevProcStats_.find(pid) != prevProcStats_.end());
        if (has_prev) {
            prev = prevProcStats_[pid];
            unsigned long long delta_time = total_time - prev.total_time;
            unsigned long long delta_total = total_cpu_time - prevTotalCpuTime_;
            double usage = 100.0 * delta_time / (double)delta_total;
            if (delta_total == 0) usage = 0;
            proc_cpu_usage.emplace_back(pid, usage);
        }

        current_proc_stats[pid] = {utime, stime, cutime, cstime, starttime, total_time};
        // Also read RSS from status (more accurate) but we use rss from stat (in pages -> KB)
        long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
        unsigned long long rss_kb = rss * page_size_kb;
        // For top processes, we will fill later after sorting
    }
    closedir(dir);

    // Sort by CPU usage descending
    std::sort(proc_cpu_usage.begin(), proc_cpu_usage.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    stats->num_top_procs = 0;
    for (size_t i = 0; i < proc_cpu_usage.size() && i < MAX_TOP_PROCS; ++i) {
        int pid = proc_cpu_usage[i].first;
        double cpu = proc_cpu_usage[i].second;
        // Get process name again (already had from parsing, but we store)
        std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream stat_file(stat_path);
        if (!stat_file) continue;
        std::string line;
        std::getline(stat_file, line);
        size_t first = line.find('(');
        size_t last = line.rfind(')');
        if (first == std::string::npos || last == std::string::npos) continue;
        std::string name = line.substr(first+1, last-first-1);
        // Get RSS
        unsigned long long rss_kb = 0;
        std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
        std::string status_line;
        while (std::getline(status_file, status_line)) {
            if (status_line.compare(0, 6, "VmRSS:") == 0) {
                sscanf(status_line.c_str(), "VmRSS: %llu kB", &rss_kb);
                break;
            }
        }
        auto& proc = stats->top_procs[stats->num_top_procs];
        proc.pid = pid;
        strncpy(proc.name, name.c_str(), PROC_NAME_LEN-1);
        proc.name[PROC_NAME_LEN-1] = '\0';
        proc.cpu_usage = cpu;
        proc.memory_rss_kb = rss_kb;
        stats->num_top_procs++;
    }

    prevProcStats_ = std::move(current_proc_stats);
    prevTotalCpuTime_ = total_cpu_time;
}

double StatsCollector::calculateCpuUsage(const CpuPrev& prev, const CpuPrev& cur) {
    unsigned long long prev_idle = prev.idle + prev.iowait;
    unsigned long long cur_idle = cur.idle + cur.iowait;
    unsigned long long prev_non_idle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
    unsigned long long cur_non_idle = cur.user + cur.nice + cur.system + cur.irq + cur.softirq + cur.steal;
    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long cur_total = cur_idle + cur_non_idle;
    unsigned long long total_delta = cur_total - prev_total;
    unsigned long long idle_delta = cur_idle - prev_idle;
    if (total_delta == 0) return 0.0;
    return 100.0 * (total_delta - idle_delta) / total_delta;
}