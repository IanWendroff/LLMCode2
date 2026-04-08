// struct NodeInfo{
//     std::string ip;
//     int port;
//  };
#include "config.h"
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

struct KeyLock {
    std::mutex mtx;
    std::condition_variable cv;
    int readers = 0;
    bool writer = false;
};

// extern int server_fd;

KeyLock& get_lock(int key);
 
// int self_id;

static std::unordered_map<int, std::unique_ptr<KeyLock>> key_locks;
static std::mutex key_locks_mutex;

int parse_key(const std::string& line);
void parse_put(const std::string& line, int& key, std::string& value);

void acquire_read(int key);

void release_read(int key);

void acquire_write(int key);

void dht_init(int node_id);

void release_write(int key);
int owner_node(int key);
bool dht_put(int key, const std::string& value);
bool put_many(std::vector<std::pair<int, std::string>> vec);
std::optional<std::string> dht_get(int key);
void handle_client(int sock);

void server_loop();
void dht_printall();