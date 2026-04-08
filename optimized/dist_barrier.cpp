#include "dist_barrier.h"
#include "network.h"
#include "dht.h"
#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <thread>
#include <iostream>
#include <mutex>
#include <atomic>

std::mutex barrier_mtx;
std::atomic<int> barrier_ready{0};

// extern int self_id;
// extern NodeInfo nodes[NUM_NODES];

//client-only dist barrier

void distributed_barrier()
{
    std::cout << "[Node " << self_id << "] Entering distributed_barrier()\n";
    if (self_id == 0) {
        while (true) {
            std::lock_guard<std::mutex> lock(barrier_mtx);
            if (barrier_ready >= NUM_NODES - 1)
                break;
        }
        return;
        // std::this_thread::sleep_for(std::chrono::milliseconds(1)); //todo: is this correct? or return() instead?
    }
    int sock = -1;
    while (sock < 0) {
        std::cout << "[Node" << self_id << "] trying to connect to Node 0\n";
        std::cout.flush();
        
        sock = connect_to(nodes[0].ip, nodes[0].port);
        if (sock < 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    send_line(sock, "BARRIER READY\n");

    std::string msg;
    if (!recv_line(sock, msg) || msg != "BARRIER GO\n") {
        std::cerr << "[Node " << self_id << "] Barrier failed\n";
        exit(1);
    }

    close(sock);
}
