#include "dht.h"
#include "dist_barrier.h"
#include "network.h"
#include <random>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <atomic>
#include <signal.h>

using namespace std;

// Each node has metrics
std::atomic<long> put_success{0};
std::atomic<long> put_fail{0};
std::atomic<long> multiput_success{0};
std::atomic<long> multiput_fail{0};
std::atomic<long> get_hit{0};
std::atomic<long> get_miss{0};

std::atomic<long> total_ops{0};
std::atomic<long long> total_latency_ns{0};

void worker(int operations, int key_range){
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> key_dist(0, key_range - 1);
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);

    for (int i = 0; i < operations; i++) {

        int key = key_dist(rng);
        double p = op_dist(rng);

        auto start = std::chrono::high_resolution_clock::now();

        if (p < 0.2) {  // 20% puts

            vector<pair<int, string>> single_key; //in vector form for put_many format
            single_key.reserve(1); //allocate mem

            single_key.emplace_back(key, "val_"+to_string(key));
            string kv_string;
            bool ok = put_many(single_key);

            if (ok) {
                std::cout << "Success putting " << to_string(key) << "\n";
                ++put_success;
                total_ops++;
            } else {
                ++put_fail;
            }
        }
        else if (p < 0.4){ //multi put
            int count = 2+ (rng() % 2); //random, from 2 to 4

            vector<pair<int, string>> kvs;
            kvs.reserve(count); //allocate mem

            for (int j = 0; j<count;j++){
                int k = key_dist(rng); //problem: didn't check that it isnt duplicate key. update: fixed
                bool duplicate = false;
                for (auto &p : kvs){
                    if (p.first == k){
                        duplicate = true;
                        j--;
                    }
                }
                if (duplicate == false){
                    kvs.emplace_back(k, "val_"+to_string(k));
                }    
            }
            string kv_string;
            for (auto p : kvs){
                kv_string += to_string(p.first) + " " + p.second + ", "; 
            }
            cout << "Attempting multiput: " << kv_string << "\n";
            bool ok = put_many(kvs);
            if(ok){
                std::cout << "Success multiputting " << kv_string << "\n";
                ++ multiput_success;
                total_ops++;
            }
            else{
                // std::cout << "Fail multiputting " << kv_string << "\n";
                ++multiput_fail;
            }
        }
        else { // 60% gets
            auto val = dht_get(key);
            if (val.has_value()) {
                ++get_hit;
            } else {
                ++get_miss;
            }
            total_ops++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        long long latency =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        total_latency_ns += latency;
        // total_ops++; //moved to only count successes.. idk. and count all gets
    }
}

int main(int argc, char** argv)
{
    //prevents prog from dying if broken pipe happens (node tries to call send_line on closed socket)
    signal(SIGPIPE, SIG_IGN); //send will return -1 if socket closed

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <node_id> <num_threads> <num_ops>\n";
        return 1;
    }

    self_id = std::stoi(argv[1]);
    int num_threads = std::stoi(argv[2]);
    int num_ops = std::stoi(argv[3]);

    load_config();
    dht_init(self_id);

    // Start server thread first
    std::thread(server_loop).detach();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Synchronize all nodes
    std::cout << "[Node " << self_id << "] Attempting barrier\n";
    distributed_barrier();
    std::cout << "[Node " << self_id << "] Passed barrier\n";

    // std::vector<int> key_ranges = {1000, 10000};
    std::vector<int> key_ranges = {100};

    for (int key_range : key_ranges) {

        put_success = 0;
        put_fail = 0;
        get_hit = 0;
        get_miss = 0;
        total_ops = 0;
        total_latency_ns = 0;

        int ops_per_thread = num_ops / num_threads;

        std::vector<std::thread> workers;
        workers.reserve(num_threads); //allocate memory for threads

        auto experiment_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_threads; i++) {
            workers.emplace_back(worker, ops_per_thread, key_range);
        }

        for (auto &t : workers) {
            t.join();
        }

        auto experiment_end = std::chrono::high_resolution_clock::now();
        double elapsed_sec =
            std::chrono::duration_cast<std::chrono::duration<double>>(experiment_end - experiment_start).count();

        double throughput = total_ops.load() / elapsed_sec;
        double avg_latency_us =
            (total_latency_ns.load() / 1000.0) / total_ops.load();

        std::cout << "Node " << self_id
                  << " finished key_range TEST " << key_range << "\n";
                  

        std::ofstream out("metrics_node_" + std::to_string(self_id) +
                          "_keyrange_" + std::to_string(key_range) + ".csv");

        out << "node_id,key_range,throughput_ops_sec,avg_latency_us,total_ops\n";
        out << self_id << ","
            << key_range << ","
            << throughput << ","
            << avg_latency_us << ","
            << total_ops.load() << "\n";

        std::cout << "Node " << self_id
                  << " wrote to file\n";
        
        // distributed_barrier();
        std::this_thread::sleep_for(std::chrono::seconds(10));
        out.close();
        std::cout << "Node " << self_id
                  << " closed file\n";
        
    }
    return 0;
}