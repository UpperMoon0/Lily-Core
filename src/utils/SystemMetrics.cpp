#include "lily/utils/SystemMetrics.hpp"
#include "lily/services/Service.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <cpprest/http_client.h>

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

std::vector<ServiceStatus> SystemMetricsCollector::get_service_statuses(lily::services::Service* tool_service) {
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
    
    // Add discovered services from Service class
    if (tool_service) {
        const auto& service_infos = tool_service->get_services_info();
        auto discovered_servers = tool_service->get_discovered_servers();
        auto tools_per_server = tool_service->get_tools_per_server();
        size_t total_tools = tool_service->get_tool_count();
        
        for (const auto& service_info : service_infos) {
            ServiceStatus service_status;
            service_status.name = service_info.name;
            service_status.details["http_url"] = service_info.http_url;

            if (service_info.mcp) {
                bool is_active = std::find(discovered_servers.begin(), discovered_servers.end(), service_info.http_url) != discovered_servers.end();
                service_status.status = is_active ? "healthy" : "down";
                service_status.details["type"] = "MCP Server";
                if (is_active) {
                    auto it = tools_per_server.find(service_info.http_url);
                    if (it != tools_per_server.end()) {
                        service_status.details["tool_count"] = std::to_string(it->second.size());
                        
                        // Add tool names and descriptions
                        std::string tools_str;
                        for (const auto& tool : it->second) {
                            if (tool.contains("name")) {
                                std::string name = tool["name"].get<std::string>();
                                std::string description = tool.value("description", "");
                                if (!tools_str.empty()) {
                                    tools_str += "|";
                                }
                                tools_str += name + ":" + description;
                            }
                        }
                        if (!tools_str.empty()) {
                            service_status.details["tools"] = tools_str;
                        }
                    }
                }
            } else {
                // For non-MCP services, perform an actual health check
                bool is_healthy = check_service_health(service_info.http_url);
                service_status.status = is_healthy ? "healthy" : "down";
                service_status.details["type"] = "Registered Service";
                if (!is_healthy) {
                    service_status.details["error"] = "Service is not responding";
                }
            }
            
            service_status.last_updated = core_service.last_updated;
            services.push_back(service_status);
        }
        
        // Add summary information
        ServiceStatus mcp_summary;
        mcp_summary.name = "MCP Services Summary";
        mcp_summary.status = "healthy";
        mcp_summary.details["active_mcp_servers"] = std::to_string(discovered_servers.size());
        mcp_summary.details["total_tools"] = std::to_string(total_tools);
        mcp_summary.details["description"] = "Summary of all MCP services";
        mcp_summary.last_updated = core_service.last_updated;
        services.push_back(mcp_summary);
    }
    
    return services;
}

MonitoringData SystemMetricsCollector::get_monitoring_data(const std::string& service_name, const std::string& version, lily::services::Service* tool_service) {
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

bool SystemMetricsCollector::check_service_health(const std::string& service_url) {
    try {
        // Create HTTP client
        web::uri service_uri(utility::conversions::to_string_t(service_url));
        web::uri_builder builder(service_uri);
        builder.append_path(U("/health"));
        
        web::http::client::http_client client(builder.to_uri());
        
        // Send a GET request to the /health endpoint
        web::http::http_request request(web::http::methods::GET);
        auto response = client.request(request).get();
        
        // If we get a 200 OK response, the service is considered healthy
        return response.status_code() == web::http::status_codes::OK;
    } catch (const std::exception& e) {
        // If there's any exception (connection refused, timeout, etc.), the service is down
        return false;
    }
}

} // namespace utils
} // namespace lily