#pragma once

#include "../common/SharedStruct.h"
#include "../common/SharedMemory.h"
#include <string>

class WebServer {
public:
    WebServer(const std::string& static_dir, int port = 8080);
    void run();

private:
    std::string static_dir_;
    int port_;
    SharedMemory shm_;
    SharedStats* stats_;

    std::string getStatsJson();
    static void handleApiStats(const httplib::Request& req, httplib::Response& res, void* userdata);
    static void handleStatic(const httplib::Request& req, httplib::Response& res, void* userdata);
};