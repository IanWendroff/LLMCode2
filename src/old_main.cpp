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

//each node has metrics
std::atomic<long> put_success{0};
std::atomic<long> put_fail{0};
std::atomic<long> get_hit{0};
std::atomic<long> get_miss{0};

std::atomic<long> total_ops{0};
std::atomic<long long> total_latency_ns{0};

void worker(int operations, int key_range){
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> key_dist(0, key_range - 1); //key distribution
    std::uniform_real_distribution<double> op_dist(0.0, 1.0); 

    for (int i =0; i<operations; i++){
        int key = key_dist(rng);
        double p = op_dist(rng);

        auto start = std::chrono::high_resolution_clock::now(); //measure time

        if (p < 0.2){ //20% puts
            bool ok = dht_put(key, "val_" + std::to_string(key));
            // int target_node = owner_node(key);
            if(ok) {
                long v = ++put_success;
                std::cout << "[Node " << self_id << "] "
                  << "PUT_OK=" << v << " "
                  << "PUT_FAIL=" << put_fail.load() << " "
                  << "GET_HIT=" << get_hit.load() << " "
                  << "GET_MISS=" << get_miss.load() << "\n";
            }
            else{
                //unsuccessful
                long v = ++put_fail;
            //     std::cout << "[Node " << self_id << "] "
            //         << "PUT_OK=" << put_success.load() << " "
            //         << "PUT_FAIL=" << v << " "
            //         << "GET_HIT=" << get_hit.load() << " "
            //         << "GET_MISS=" << get_miss.load() << "\n";
            }
            // std::cout << "[Node " << self_id << "] PUT " << key << " -> " << "val_" + std::to_string(key) << " routed to node: " << target_node << " \n"; //print node that was put
        }
        else{ //80% gets
            auto val = dht_get(key);
            if (val.has_value()){
                long v = ++get_hit;
                std::cout << "[Node " << self_id << "] "
                        << "PUT_OK=" << put_success.load() << " "
                        << "PUT_FAIL=" << put_fail.load() << " "
                        << "GET_HIT=" << v << " "
                        << "GET_MISS=" << get_miss.load() << "\n";
            }
            else{
                //unsuccessful get, don't print
                long v = ++get_miss;
                // std::cout << "[Node " << self_id << "] "
                //   << "PUT_OK=" << put_success.load() << " "
                //   << "PUT_FAIL=" << put_fail.load() << " "
                //   << "GET_HIT=" << get_hit.load() << " "
                //   << "GET_MISS=" << v << "\n";

            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        long long latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        total_latency_ns += latency;
        total_ops++;

    }
}

int main(int argc, char** argv)
{
    self_id = std::stoi(argv[1]); //id of manager node
    int num_threads = std::stoi(argv[2]);
    int num_ops = std::stoi(argv[3]);



    load_config();

    dht_init(self_id);
    std::thread(server_loop).detach();//start dht server
    std::this_thread::sleep_for(std::chrono::seconds(1));
    distributed_barrier(); //make sure all are synced
    
    std::vector<int> key_ranges = {10, 100, 1000, 10000};

    for (int key_range : key_ranges) {
        put_success = put_fail = get_hit = get_miss = 0;
        total_ops = 0;
        total_latency_ns = 0;

        // distributed_barrier();
        auto experiment_start = std::chrono::high_resolution_clock::now();


        if (argc < 4){
            std::cerr << "usage: " << argv[0]
                    << " <node_id> <num_threads> <num_ops>\n";
            return 1; //return error, too few nodes
        }
    

        std::vector<std::thread> workers;  
        int ops_per_thread = num_ops / num_threads;
        // int key_range = 1000; //todo: change

        std::cout << "[Node " << self_id << "] Starting server on IP: " << nodes[self_id].ip << " on port " << nodes[self_id].port << "\n";
            std::cout.flush();
        std::cout << "Listening on port " << nodes[self_id].port << std::endl;


        for (int i = 0; i<num_threads; i++){
            workers.emplace_back(worker, ops_per_thread, key_range);
            
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        for (int i = 0; i < NUM_NODES; i++) {
            if (self_id == i) {
                dht_printall();
            }
        }

        for (auto & t: workers){
            t.join();
        }

        auto experiment_end = std::chrono::high_resolution_clock::now();

        double elapsed_sec =
            std::chrono::duration_cast<std::chrono::duration<double>>(experiment_end - experiment_start).count();

        double throughput = total_ops.load() / elapsed_sec;
        double avg_latency_us =
            (total_latency_ns.load() / 1000.0) / total_ops.load();

        std::cout << "Node " << self_id << " finished workload\n";

        std::ofstream out("metrics_node_" + std::to_string(self_id) +
                    "_keys_" + std::to_string(key_range) + ".csv");

        out << "node_id,key_range,throughput_ops_sec,avg_latency_us,total_ops\n";
        out << self_id << ","
            << key_range << ","
            << throughput << ","
            << avg_latency_us << ","
            << total_ops.load() << "\n";

        out.close();

    }
    return 0;
}