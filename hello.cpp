#include "httplib.h"
#include <winsock2.h>
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include "toml.hpp"
#include <filesystem>
#include <chrono>
#include <list>
#include <sstream>
#include <iomanip>

// Global variables
SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus = NULL;

// Function declarations
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
void WINAPI ServiceCtrlHandler(DWORD dwCtrl);

// Function to log messages to a file
void LogToFile(const std::string &message)
{
    std::ofstream logFile;
    logFile.open("C:/rust_teszt/log.txt", std::ios_base::app); // Open in append mode

    if (logFile.is_open())
    {
        // Write the log message
        logFile << message << std::endl;
        logFile.close();
    }
}

auto ReadConfig(const std::string &filename)
{
    try
    {
        auto config = toml::parse_file(filename); // Parse the TOML file

        // std::string_view workstation_name = config["config"]["workstation"].value_or("faszom");

        std::string workstation_name = config["config"]["workstation"].value_or("faszom1");
        std::string folder = config["config"]["folder"].value_or("faszom2");
        std::string uploader_endpoint = config["config"]["uploader_endpoint"].value_or("faszom3");
        std::string checker_endpoint = config["config"]["checker_endpoint"].value_or("faszom3");
        std::string days = config["config"]["days"].value_or("faszom5");

        // Log the parsed values
        LogToFile("Config loaded:");
        LogToFile("Workstation: " + workstation_name);
        LogToFile("folder: " + folder);
        LogToFile("uploader_endpoint: " + uploader_endpoint);
        LogToFile("checker_endpoint: " + checker_endpoint);
        LogToFile("days: " + days);

        return config;
    }
    catch (const toml::parse_error &err)
    {
        LogToFile("error Config loaded:");
        LogToFile(std::string(err.description()));
        std::cerr
            << "Error reading TOML file: " << err.description() << std::endl;
    }
}

namespace fs = std::filesystem;

struct FileInfo
{
    std::string file_name;
    std::chrono::system_clock::time_point file_modification_date;
};

struct WorkstationFiles
{
    std::string workstation;
    std::list<FileInfo> files;
};

struct RelevantFiles
{
    std::list<std::string> file_names;
};

long long toSecondsSinceEpoch(const std::chrono::system_clock::time_point &tp)
{
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

void postWorkstationFiles(const WorkstationFiles &workstationFiles, const std::string &endpoint)
{
    // Serialize the WorkstationFiles data to JSON
    std::ostringstream jsonStream;
    jsonStream << "{\"workstation\": \"" << workstationFiles.workstation << "\", \"files\": [";
    bool first = true;
    for (const auto &file : workstationFiles.files)
    {
        if (!first)
            jsonStream << ", ";
        first = false;
        jsonStream << "{\"file_name\": \"" << file.file_name << "\", \"file_modification_date\": " << toSecondsSinceEpoch(file.file_modification_date) << "}";
    }
    jsonStream << "]}";
    std::string jsonData = jsonStream.str();

    // HTTP
    httplib::Client cli("http://cpp-httplib-server.yhirose.repl.co");

    auto res = cli.Get("/hi");
    res->status;
    res->body;
}

std::vector<FileInfo> getRecentFiles(const std::string &path, int days)
{
    std::vector<FileInfo> recentFiles;
    try
    {
        // Calculate the time point for comparison
        auto now = std::chrono::system_clock::now();
        auto cutoffTime = now - std::chrono::hours(24 * days);

        // Iterate through the directory
        for (const auto &entry : fs::directory_iterator(path))
        {
            if (fs::is_regular_file(entry))
            {
                auto lastWriteTime = fs::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(lastWriteTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

                if (sctp > cutoffTime)
                {
                    recentFiles.push_back({entry.path().string(), sctp});
                }
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return recentFiles;
}

// Main entry point for the service
int _tmain(int argc, _TCHAR *argv[])
{
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {_T("MyService"), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL} // Null-terminating entry
    };

    // Start the service control dispatcher
    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        std::cerr << "Error starting service control dispatcher" << std::endl;
        return 1;
    }

    return 0;
}

// ServiceMain - Entry point for the service
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    // Register the service control handler
    hStatus = RegisterServiceCtrlHandler(_T("MyService"), ServiceCtrlHandler);
    if (hStatus == NULL)
    {
        std::cerr << "Error registering service control handler" << std::endl;
        return;
    }

    // Set the service status to running
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ServiceStatus.dwWin32ExitCode = NO_ERROR;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;

    // Report the status to the SCM
    if (SetServiceStatus(hStatus, &ServiceStatus) == FALSE)
    {
        std::cerr << "Error setting service status" << std::endl;
        return;
    }

    // Log the service startup
    LogToFile("Service started at " + std::string(__TIME__));

    // Service logic (in this case, we'll just simulate a service running)
    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        auto config = ReadConfig("C:/rust_teszt/config.toml");

        // Log every minute (60 seconds)

        auto files = getRecentFiles(config["config"]["folder"].value_or(""), atoi(config["config"]["days"].value_or("1")));

        std::cout << "Files modified in the last " << config["config"]["folder"] << " days:" << std::endl;
        for (const auto &file : files)
        {
            std::time_t modTime = std::chrono::system_clock::to_time_t(file.file_modification_date);
            std::cout << file.file_name << " (Last Modified: " << std::ctime(&modTime) << ")";

            LogToFile(file.file_name);
            LogToFile(std::ctime(&modTime));
        }

        WorkstationFiles workstationFiles;
        workstationFiles.workstation = config["config"]["workstation"].value_or("");
        workstationFiles.files = std::list<FileInfo>(files.begin(), files.end());

        std::cout << "Posting files to the endpoint...\n";
        postWorkstationFiles(workstationFiles, config["config"]["checker_endpoint"].value_or(""));

        //
        LogToFile("Service is running... Time: 1 minute(s)");

        // Simulate doing work and wait for 60 seconds
        Sleep(60000); // Sleep for 60 seconds
    }

    // After work, change service status to stop
    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);

    // Log the service stop
    LogToFile("Service stopped at " + std::string(__TIME__));
}

// ServiceCtrlHandler - Handles service control requests
void WINAPI ServiceCtrlHandler(DWORD dwCtrl)
{
    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        // Handle stop request
        ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(hStatus, &ServiceStatus);
        // Perform cleanup here
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(hStatus, &ServiceStatus);
        break;

    default:
        break;
    }
}