#include "WebServer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

WebServer::WebServer(const std::string& static_dir, int port)
    : static_dir_(static_dir), port_(port), shm_(SHM_NAME, sizeof(SharedStats), false),
      stats_(reinterpret_cast<SharedStats*>(shm_.get())) {}

std::string WebServer::getStatsJson() {
    pthread_mutex_lock(&stats_->mutex);
    json j;
    j["timestamp"] = stats_->timestamp;
    j["cpu_total"] = stats_->cpu_usage_total;
    j["num_cpus"] = stats_->num_cpus;
    for (int i = 0; i < stats_->num_cpus; ++i) {
        j["cpu_per_core"].push_back(stats_->cpu_usage_per_core[i]);
    }
    j["memory"]["total_kb"] = stats_->mem_total_kb;
    j["memory"]["used_kb"] = stats_->mem_used_kb;
    j["memory"]["free_kb"] = stats_->mem_free_kb;
    j["memory"]["available_kb"] = stats_->mem_available_kb;
    j["swap"]["total_kb"] = stats_->swap_total_kb;
    j["swap"]["used_kb"] = stats_->swap_used_kb;
    j["load_avg"][0] = stats_->load_avg_1;
    j["load_avg"][1] = stats_->load_avg_5;
    j["load_avg"][2] = stats_->load_avg_15;
    j["num_top_procs"] = stats_->num_top_procs;
    for (int i = 0; i < stats_->num_top_procs; ++i) {
        json proc;
        proc["pid"] = stats_->top_procs[i].pid;
        proc["name"] = stats_->top_procs[i].name;
        proc["cpu_usage"] = stats_->top_procs[i].cpu_usage;
        proc["memory_rss_kb"] = stats_->top_procs[i].memory_rss_kb;
        j["top_procs"].push_back(proc);
    }
    pthread_mutex_unlock(&stats_->mutex);
    return j.dump();
}

void WebServer::handleApiStats(const httplib::Request&, httplib::Response& res, void* userdata) {
    WebServer* self = static_cast<WebServer*>(userdata);
    res.set_content(self->getStatsJson(), "application/json");
}

void WebServer::handleStatic(const httplib::Request& req, httplib::Response& res, void* userdata) {
    WebServer* self = static_cast<WebServer*>(userdata);
    std::string path = req.path;
    if (path == "/") path = "/index.html";
    std::string full_path = self->static_dir_ + path;
    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
        res.status = 404;
        res.set_content("Not Found", "text/plain");
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    std::string ext = path.substr(path.find_last_of('.') + 1);
    if (ext == "html") res.set_content(content, "text/html");
    else if (ext == "css") res.set_content(content, "text/css");
    else if (ext == "js") res.set_content(content, "application/javascript");
    else res.set_content(content, "text/plain");
}

void WebServer::run() {
    httplib::Server svr;
    svr.Get("/api/stats", [this](const httplib::Request& req, httplib::Response& res) {
        handleApiStats(req, res, this);
    });
    svr.Get("/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
        handleStatic(req, res, this);
    });
    std::cout << "Web server listening on http://localhost:" << port_ << std::endl;
    svr.listen("0.0.0.0", port_);
}