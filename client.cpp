#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <fstream>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <thread>
#include <regex>

struct Message {
    long mtype;
    char mtext[256];
};

key_t control_key = ftok("control_queue", 65);
int control_qid;
long client_id;
int reply_qid;
std::ofstream log_file;

// Function to extract timestamp and calculate latency
void process_message(const std::string& text) {
    auto t3 = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    
    // Regex to find the timestamp: [TS:1633512345678]
    std::regex ts_regex("\\[TS:([0-9]+)\\]");
    std::smatch match;
    
    if (std::regex_search(text, match, ts_regex) && match.size() > 1) {
        long long t1 = std::stoll(match.str(1));
        std::chrono::duration<double, std::milli> latency = std::chrono::nanoseconds(t3 - t1);
        
        // Remove the timestamp tag for display
        std::string clean_text = std::regex_replace(text, ts_regex, "");
        
        std::cout << "\n[Server] " << clean_text << " (E2E Latency: " << latency.count() << " ms)" << std::endl;
        log_file << client_id << ",\"E2E: " << clean_text << "\"," << latency.count() << "\n";
    } else {
        // Handle messages without timestamp (e.g., initial join confirmation or echo)
        std::cout << "\n[Server] " << text << std::endl;
        log_file << client_id << ",\"Reply: " << text << "\",0\n";
    }
    std::cout << ">> " << std::flush; // Reprompt the user
}

// Thread to continuously listen for incoming messages
void listen_thread() {
    Message msg;
    std::cout << "[Client] Listener started. Ready for messages..." << std::endl;
    while (true) {
        // Blockingly wait for any message destined for this client_id
        if (msgrcv(reply_qid, &msg, sizeof(msg.mtext), client_id, 0) < 0) {
            perror("msgrcv in listen_thread");
            // Only continue if error is recoverable, otherwise break loop or handle exit
            continue; 
        }
        process_message(msg.mtext);
    }
}

// Thread to handle user input and send commands
void input_thread() {
    Message msg;
    msg.mtype = client_id;

    std::string input;
    while (true) {
        std::cout << ">> ";
        std::getline(std::cin, input);
        if (input == "exit" || input == "QUIT") break;

        strncpy(msg.mtext, input.c_str(), sizeof(msg.mtext));
        msg.mtext[sizeof(msg.mtext)-1] = '\0';
        
        // Only send, listener handles receiving the reply/broadcast
        if (msgsnd(control_qid, &msg, sizeof(msg.mtext), 0) < 0) {
            perror("msgsnd");
        }
    }

    // Send QUIT command before exiting
    std::string quit_cmd = "QUIT";
    strncpy(msg.mtext, quit_cmd.c_str(), sizeof(msg.mtext));
    msgsnd(control_qid, &msg, sizeof(msg.mtext), 0);

    std::cout << "[Client] Exiting gracefully..." << std::endl;
    // Note: In a real app, you would signal the listen_thread to stop here.
    exit(0);
}


int main() {
    control_qid = msgget(control_key, 0666 | IPC_CREAT);
    if (control_qid < 0) { perror("msgget control"); return 1; }

    client_id = getpid();
    // Create the reply queue. The client_id is the key and also the mtype.
    reply_qid = msgget(client_id, 0666 | IPC_CREAT);
    if (reply_qid < 0) { perror("msgget reply"); return 1; }


    log_file.open("client_stat_" + std::to_string(client_id) + ".csv", std::ios::app);
    log_file << "client_id,event,latency_ms\n";


    std::thread listener(listen_thread);
    std::thread inputter(input_thread);

    listener.join();
    inputter.join();

    log_file.close();
    // Cleanup the client's reply queue (optional, but good practice)
    // msgctl(reply_qid, IPC_RMID, NULL); 

    return 0;
}