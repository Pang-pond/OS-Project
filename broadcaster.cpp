#include <iostream>
#include <fstream>
#include <vector> // Changed to list in router.cpp
#include <list> // Using list
#include <mutex>
#include <chrono>
#include <string>
#include <sys/msg.h>
#include <cstring>
#include "registry.hpp"

// extern std::vector<std::pair<long, std::string>> broadcast_tasks; // Removed
extern std::list<std::pair<long, std::string>> broadcast_tasks; // Changed to list
extern std::mutex task_mutex;

void broadcaster_thread(int id) {
    std::ofstream log("broadcaster_" + std::to_string(id) + ".csv");
    log << "client_id,status,latency_ms\n"; // Modified logging format

    while (true) {
        std::pair<long, std::string> task;
        bool task_available = false;
        {
            std::unique_lock lock(task_mutex);
            if (!broadcast_tasks.empty()) {
                task = broadcast_tasks.front(); // Use front() for FIFO-like behavior
                broadcast_tasks.pop_front();
                task_available = true;
            }
        }
        if (!task_available) {
            // Sleep briefly to avoid busy-waiting and consuming excessive CPU
            std::this_thread::sleep_for(std::chrono::microseconds(1)); 
            continue;
        }

        auto start = std::chrono::high_resolution_clock::now();
        int qid = -1;
        {
            std::shared_lock lock(client_lock);
            // Check if the client is still registered
            auto it = client_registry.find(task.first);
            if (it == client_registry.end()) {
                log << task.first << ",\"Dropped (Client unregistered)\",0\n";
                continue;
            }
            qid = it->second;
        }

        struct Message {
            long mtype;
            char mtext[256];
        } msg;
        msg.mtype = task.first;
        strncpy(msg.mtext, task.second.c_str(), sizeof(msg.mtext));
        msg.mtext[sizeof(msg.mtext)-1] = '\0';

        // *** CRITICAL CHANGE: Use IPC_NOWAIT to prevent blocking if the queue is full ***
        if (msgsnd(qid, &msg, sizeof(msg.mtext), IPC_NOWAIT) < 0) {
            // Error handling for message drop (e.g., EWOULDBLOCK means queue is full)
            if (errno == EAGAIN) { // EAGAIN is often EWOULDBLOCK on some systems
                std::cerr << "[Broadcaster " << id << "] Warning: Message to " << task.first << " dropped (Queue full/slow client)." << std::endl;
                log << task.first << ",\"Dropped (Queue full)\",0\n";
            } else {
                 // Other errors like queue removed
                std::cerr << "[Broadcaster " << id << "] Error sending to " << task.first << ": " << strerror(errno) << std::endl;
                log << task.first << ",\"Error: " << strerror(errno) << "\",0\n";
            }
            continue; 
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> latency = end - start;
        log << task.first << ",\"Sent successfully\"," << latency.count() << "\n";
    }
}