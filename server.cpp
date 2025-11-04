#include <iostream>
#include <thread>
#include <vector>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "router.cpp"
#include "broadcaster.cpp"

int main() {
    key_t control_key = ftok("control_queue", 65);
    int control_qid = msgget(control_key, 0666 | IPC_CREAT);
    if (control_qid < 0) { perror("msgget control"); return 1; }

    std::cout << "Server started. Control Queue ID: " << control_qid << std::endl;

    std::thread router(router_thread, control_qid);
    int num_broadcasters = 4;
    std::vector<std::thread> broadcasters;
    for (int i = 0; i < num_broadcasters; ++i)
        broadcasters.emplace_back(broadcaster_thread, i+1);

    router.join();
    for (auto& t : broadcasters) t.join();
}