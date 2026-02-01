#ifndef LILY_UTILS_SYSTEM_METRICS_HPP
#define LILY_UTILS_SYSTEM_METRICS_HPP

#include <string>
#include <vector>
#include <chrono>
#include <map>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace lily {
namespace services {
    class Service;  // Forward declaration
}

namespace utils {

struct SystemMetrics {
    double cpu_usage;
    double memory_usage;
    double disk_usage;
    std::string uptime;
};

struct MonitoringData {
    std::string status;
    std::string service_name;
    std::string version;
    std::string timestamp;
    SystemMetrics metrics;
    std::map<std::string, std::string> details;
};

class SystemMetricsCollector {
public:
    SystemMetricsCollector();
    ~SystemMetricsCollector();
    
    SystemMetrics get_system_metrics();
    MonitoringData get_monitoring_data(const std::string& service_name, const std::string& version);
    
private:
    std::chrono::steady_clock::time_point start_time;
    
#ifdef _WIN32
    ULARGE_INTEGER last_cpu_time;
    ULARGE_INTEGER last_sys_cpu_time;
    ULARGE_INTEGER last_user_cpu_time;
    int num_processors;
    
    void init_cpu_monitoring();
    double get_cpu_usage();
    double get_memory_usage();
    double get_disk_usage();
    std::string get_uptime();
#endif
};

} // namespace utils
} // namespace lily

#endif // LILY_UTILS_SYSTEM_METRICS_HPP