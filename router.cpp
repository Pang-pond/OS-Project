#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <sys/msg.h>
#include <cstring>
#include <chrono>
#include <list>
#include "registry.hpp"

// ... (Existing struct Message)
struct Message {
    long mtype;
    char mtext[256];
};

// Use std::list for broadcast tasks for better queue semantics/performance over vector
// The pair now stores: (TargetClientID, MessageWithTimestamp)
std::list<std::pair<long, std::string>> broadcast_tasks; 
std::mutex task_mutex;

// Helper function to create the message with a start timestamp (T1)
std::string create_stamped_message(const std::string& content) {
    // T1: Time when the router processed the command
    auto t1 = std::chrono::high_resolution_clock::now().time_since_epoch().count(); 
    return "[TS:" + std::to_string(t1) + "]" + content;
}

void router_thread(int control_qid) {
    Message msg;
    while (true) {
        if (msgrcv(control_qid, &msg, sizeof(msg.mtext), 0, 0) < 0) continue;
        // Comment out excessive log to reduce console I/O overhead for better performance testing
        // std::cout << "[Router] Received from client " << msg.mtype << ": " << msg.mtext << std::endl;

        std::string text(msg.mtext);
        std::istringstream iss(text);
        std::string cmd;
        iss >> cmd;
        
        // Get T1 immediately after processing the command

        if (cmd == "JOIN") {
            std::string room;
            iss >> room;
            {
                std::unique_lock lock(room_lock);
                room_registry[room].insert(msg.mtype);
            }
            {
                std::unique_lock lock(client_lock);
                // Note: The client already created the queue, here we just save the ID
                client_registry[msg.mtype] = msgget(msg.mtype, 0); 
            }
            // System notice is stamped
            std::string notice = create_stamped_message("System: client " + std::to_string(msg.mtype) + " joined " + room);
            
            std::unique_lock lock(task_mutex);
            for (long cid : room_registry[room])
                broadcast_tasks.emplace_back(cid, notice);
        }
        else if (cmd == "SAY") {
            std::string room;
            iss >> room;
            std::string content;
            std::getline(iss, content);
            // Message content is stamped
            std::string stamped_content = create_stamped_message("Room " + room + ": " + std::to_string(msg.mtype) + content);

            std::unique_lock lock(task_mutex);
            for (long cid : room_registry[room])
                broadcast_tasks.emplace_back(cid, stamped_content);
        }
        else if (cmd == "DM") {
            long target;
            iss >> target;
            std::string content;
            std::getline(iss, content);
            // DM content is stamped
            std::string stamped_content = create_stamped_message("DM from " + std::to_string(msg.mtype) + content);
            
            std::unique_lock lock(task_mutex);
            broadcast_tasks.emplace_back(target, stamped_content);
        }
        else if (cmd == "WHO") {
            std::string room;
            iss >> room;
            std::string list = "Members in " + room + ": ";
            {
                std::shared_lock lock(room_lock);
                for (long cid : room_registry[room])
                    list += std::to_string(cid) + " ";
            }
            // Reply is stamped
            std::string stamped_list = create_stamped_message(list);
            
            std::unique_lock lock(task_mutex);
            broadcast_tasks.emplace_back(msg.mtype, stamped_list);
        }
        else if (cmd == "LEAVE") {
            std::string room;
            iss >> room;
            {
                std::unique_lock lock(room_lock);
                room_registry[room].erase(msg.mtype);
            }
            // System notice is stamped
            std::string notice = create_stamped_message("System: client " + std::to_string(msg.mtype) + " left " + room);
            
            std::unique_lock lock(task_mutex);
            for (long cid : room_registry[room])
                broadcast_tasks.emplace_back(cid, notice);
        }
        else if (cmd == "QUIT") {
            {
                std::unique_lock lock(client_lock);
                client_registry.erase(msg.mtype);
            }
            {
                std::unique_lock lock(room_lock);
                for (auto& [room, members] : room_registry)
                    members.erase(msg.mtype);
            }
            // No message is sent back for QUIT
        }
        else {
            // Echo command (for unhandled commands) is stamped
            std::string stamped_echo = create_stamped_message("Echo: " + text);
            std::unique_lock lock(task_mutex);
            broadcast_tasks.emplace_back(msg.mtype, stamped_echo);
        }
    }
}