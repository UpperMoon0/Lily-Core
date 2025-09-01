#include "lily/utils/SystemMetrics.hpp"
#include "lily/services/ToolService.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace lily {
namespace utils {

SystemMetricsCollector::SystemMetricsCollector() : start_time(std::chrono::steady_clock::now()) {
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    num_processors = sys_info.dwNumberOfProcessors;
    
    // Initialize CPU monitoring
    init_cpu_monitoring();
#endif
}

SystemMetricsCollector::~SystemMetricsCollector() {
    // Cleanup if needed
}

#ifdef _WIN32
void SystemMetricsCollector::init_cpu_monitoring() {
    FILETIME ftime, fsys, fuser;
    GetSystemTimeAsFileTime(&ftime);
    last_cpu_time = *reinterpret_cast<ULARGE_INTEGER*>(&ftime);
    
    HANDLE hProcess = GetCurrentProcess();
    GetProcessTimes(hProcess, &ftime, &ftime, &fsys, &fuser);
    last_sys_cpu_time = *reinterpret_cast<ULARGE_INTEGER*>(&fsys);
    last_user_cpu_time = *reinterpret_cast<ULARGE_INTEGER*>(&fuser);
}

double SystemMetricsCollector::get_cpu_usage() {
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;
    
    GetSystemTimeAsFileTime(&ftime);
    now = *reinterpret_cast<ULARGE_INTEGER*>(&ftime);
    
    HANDLE hProcess = GetCurrentProcess();
    GetProcessTimes(hProcess, &ftime, &ftime, &fsys, &fuser);
    sys = *reinterpret_cast<ULARGE_INTEGER*>(&fsys);
    user = *reinterpret_cast<ULARGE_INTEGER*>(&fuser);
    
    percent = (sys.QuadPart - last_sys_cpu_time.QuadPart) +
              (user.QuadPart - last_user_cpu_time.QuadPart);
    percent /= (now.QuadPart - last_cpu_time.QuadPart);
    percent /= num_processors;
    percent *= 100;
    
    last_cpu_time = now;
    last_sys_cpu_time = sys;
    last_user_cpu_time = user;
    
    return percent;
}

double SystemMetricsCollector::get_memory_usage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        SIZE_T total_mem = pmc.WorkingSetSize;
        // Convert to MB and then to percentage (assuming 8GB total memory for calculation)
        // In a real implementation, you would get the actual total system memory
        return (static_cast<double>(total_mem) / (1024 * 1024)) / 8192.0 * 100.0;
    }
    return 0.0;
}

double SystemMetricsCollector::get_disk_usage() {
    // This is a simplified implementation that checks the C: drive
    ULARGE_INTEGER free_bytes, total_bytes, total_free_bytes;
    if (GetDiskFreeSpaceEx(L"C:\\", &free_bytes, &total_bytes, &total_free_bytes)) {
        if (total_bytes.QuadPart > 0) {
            double used = static_cast<double>(total_bytes.QuadPart - free_bytes.QuadPart);
            double total = static_cast<double>(total_bytes.QuadPart);
            return (used / total) * 100.0;
        }
    }
    return 0.0;
}

std::string SystemMetricsCollector::get_uptime() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    return std::to_string(duration);
}
#endif

SystemMetrics SystemMetricsCollector::get_system_metrics() {
    SystemMetrics metrics;
    
#ifdef _WIN32
    metrics.cpu_usage = get_cpu_usage();
    metrics.memory_usage = get_memory_usage();
    metrics.disk_usage = get_disk_usage();
    metrics.uptime = get_uptime();
#else
    // Default values for non-Windows systems
    metrics.cpu_usage = 0.0;
    metrics.memory_usage = 0.0;
    metrics.disk_usage = 0.0;
    metrics.uptime = "0";
#endif
    
    return metrics;
}

std::vector<ServiceStatus> SystemMetricsCollector::get_service_statuses(lily::services::ToolService* tool_service) {
    std::vector<ServiceStatus> services;
    
    // Add status for main services
    ServiceStatus core_service;
    core_service.name = "Lily-Core";
    core_service.status = "healthy";
    core_service.details["description"] = "Main application service";
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    core_service.last_updated = std::ctime(&time_t);
    // Remove newline character at the end of ctime
    if (!core_service.last_updated.empty()) {
        core_service.last_updated.pop_back();
    }
    services.push_back(core_service);
    
    ServiceStatus chat_service;
    chat_service.name = "Chat Service";
    chat_service.status = "healthy";
    chat_service.details["description"] = "Handles chat interactions";
    chat_service.last_updated = core_service.last_updated;
    services.push_back(chat_service);
    
    ServiceStatus memory_service;
    memory_service.name = "Memory Service";
    memory_service.status = "healthy";
    memory_service.details["description"] = "Manages conversation memory";
    memory_service.last_updated = core_service.last_updated;
    services.push_back(memory_service);
    
    ServiceStatus tts_service;
    tts_service.name = "TTS Service";
    tts_service.status = "healthy";
    tts_service.details["description"] = "Text-to-speech processing";
    tts_service.last_updated = core_service.last_updated;
    services.push_back(tts_service);
    
    // Add tool discovery service status
    ServiceStatus tool_service_status;
    tool_service_status.name = "Tool Discovery Service";
    tool_service_status.status = "healthy";
    tool_service_status.details["description"] = "Discovers and manages external tools";
    
    if (tool_service) {
        size_t tool_count = tool_service->get_tool_count();
        auto discovered_servers = tool_service->get_discovered_servers();
        auto tools_per_server = tool_service->get_tools_per_server();
        
        tool_service_status.details["discovered_tools"] = std::to_string(tool_count);
        tool_service_status.details["active_servers"] = std::to_string(discovered_servers.size());
        
        if (!discovered_servers.empty()) {
            std::string servers_str;
            for (size_t i = 0; i < discovered_servers.size(); ++i) {
                if (i > 0) servers_str += ", ";
                servers_str += discovered_servers[i];
            }
            tool_service_status.details["server_list"] = servers_str;
            
            // Add detailed tool information per server
            for (const auto& server_entry : tools_per_server) {
                const std::string& server_url = server_entry.first;
                const std::vector<nlohmann::json>& tools = server_entry.second;
                
                // Create a key for this server's tools
                std::string server_key = "server_" + std::to_string(std::hash<std::string>{}(server_url)) + "_tools";
                std::string tools_info = server_url + ": " + std::to_string(tools.size()) + " tools";
                
                // Add tool names
                if (!tools.empty()) {
                    tools_info += " [";
                    for (size_t i = 0; i < tools.size(); ++i) {
                        if (i > 0) tools_info += ", ";
                        if (tools[i].contains("name")) {
                            tools_info += tools[i]["name"].get<std::string>();
                        } else {
                            tools_info += "unnamed_tool";
                        }
                    }
                    tools_info += "]";
                }
                
                tool_service_status.details[server_key] = tools_info;
            }
        }
    } else {
        tool_service_status.details["discovered_tools"] = "0";
        tool_service_status.details["active_servers"] = "0";
        tool_service_status.details["server_list"] = "No tool service provided";
    }
    
    tool_service_status.last_updated = core_service.last_updated;
    services.push_back(tool_service_status);
    
    return services;
}

MonitoringData SystemMetricsCollector::get_monitoring_data(const std::string& service_name, const std::string& version, lily::services::ToolService* tool_service) {
    MonitoringData data;
    
    data.status = "healthy";
    data.service_name = service_name;
    data.version = version;
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    data.timestamp = std::ctime(&time_t);
    // Remove newline character at the end of ctime
    if (!data.timestamp.empty()) {
        data.timestamp.pop_back();
    }
    
    data.metrics = get_system_metrics();
    data.services = get_service_statuses(tool_service);
    
    // Add some details
    data.details["description"] = "Lily Core Monitoring Service";
    data.details["environment"] = "development";
    
    return data;
}

} // namespace utils
} // namespace lily