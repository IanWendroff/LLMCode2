#include "dht.h"
#include "ht.h"
#include "network.h"
#include "locks.h"
#include "dist_barrier.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <sstream>


using namespace std;

enum class TxnStatus {
    PREPARING,
    PREPARED,
    COMMITTED,
    ABORTED
};
struct TxnState {
    int txn_id;   
    // vector<pair<int, string>> writes; //kv pairs we're trying to write
    unordered_map<int, std::string> writes;
    TxnStatus status;
    vector<int> locked_keys;//keys that are locked so we know who to unlock if fail
};

unordered_map<int, TxnState> transactions;
mutex txn_table_mtx;

//condition variable for barrier loop
std::condition_variable barrier_cv;
// std::mutex barrier_mtx;

static HashTable<std::string>* local_table; 
// static std::unordered_map<int, KeyLock> key_locks;
// static std::mutex key_locks_mutex;

KeyLock& get_lock(int key)
{
    std::lock_guard<std::mutex> g(key_locks_mutex);
    return key_locks[key]; // creates if missing
}

void dht_init(int node_id)
{
    self_id = node_id;
    local_table = create_table<std::string>(CAPACITY);
}

//todo: double check all these lock acquire functs
void acquire_read(int key)
{
    KeyLock& lock = get_lock(key);
    std::unique_lock<std::mutex> lk(lock.mtx);
    lock.cv.wait(lk, [&]{ return !lock.writer; });
    lock.readers++;
}

void release_read(int key)
{
    KeyLock& lock = get_lock(key);
    std::unique_lock<std::mutex> lk(lock.mtx);
    lock.readers--;
    if (lock.readers == 0)
        lock.cv.notify_all();
}

void acquire_write(int key)
{
    KeyLock& lock = get_lock(key);
    std::unique_lock<std::mutex> lk(lock.mtx);
    lock.cv.wait(lk, [&]{ return !lock.writer && lock.readers == 0; });
    lock.writer = true;
}

//try to acquire lock, return true if timeout
bool acquire_write_timeout(int key, int timeout_ms)
{
    KeyLock& lock = get_lock(key);
    std::unique_lock<std::mutex> lk(lock.mtx);
    bool lock_acquired = (lock.cv.wait_for(
        lk, 
        chrono::milliseconds(timeout_ms), 
        [&]{ return !lock.writer && lock.readers == 0; })
    );
    if (!lock_acquired){
        return true; //timed out
    }
    lock.writer = true;
    return false; //did not time out
}

void release_write(int key)
{
    KeyLock& lock = get_lock(key);
    std::unique_lock<std::mutex> lk(lock.mtx);
    lock.writer = false;
    lk.unlock();
    lock.cv.notify_all();
}

int owner_node(int key)
{
    return key % NUM_NODES;
}

// bool dht_put(int key, const std::string& value)
// {
//     int owner = owner_node(key);

//     if (owner == self_id) {
//         acquire_write(key); 
//         bool ok = ht_put(local_table, key, value);
//         release_write(key);
//         return ok;
//     }
//     //wrote to self, now repl to all other nodes:
//     // rep_put(key, value, 1);
//         //rep_put checks what owner node is, sends REP to all others

//     int sock = connect_to(nodes[owner].ip, nodes[owner].port);
//     if (sock < 0) return false;

//     send_line(sock, "PUT " + std::to_string(key) + " " + value + "\n");

//     std::string response;
//     recv_line(sock, response);
//     close(sock);

//     return response == "OK\n";
// }


static std::atomic<uint8_t> local_ctr{0};

uint64_t generate_txn_id(uint8_t self_id) {
    using namespace std::chrono;
    uint64_t ts = duration_cast<milliseconds>(
                      steady_clock::now().time_since_epoch()
                  ).count();
    uint64_t ctr = local_ctr.fetch_add(1, std::memory_order_relaxed);
    return (ts << 16) | (uint64_t(self_id) << 8) | ctr;
}

bool put_many(std::vector<std::pair<int, std::string>> vec){
    
    int txn_id = generate_txn_id(self_id); 
    // std::cout << "Generated txn! " << to_string(txn_id) << "\n";
        //according to chatgpt: works for a globally unique id and less likely to crash across multiple coordinators, than just doing a counter
    
    sort(vec.begin(), vec.end());//so no deadlock

    //todo: make vector of owners (participants unordered set or map (std::unordered_map<int, std::vector<pair<int,string>>> per_owner;)) 
    //todo: add replicas to vector ((owner+1) % NUM_NODES)
    //make one sock per participant . for int node_id : participants, int sock = connect_to(...)


    unordered_map<int, vector<pair<int, string>>> participants; //participating nodes(incl owners And replicas), that we need to open connections to

    //add kv pairs to owner and replica nodes stored in participants
    for (auto &p : vec){
        int owner_n = owner_node(p.first);
        int replica_node = (owner_n + 1) % NUM_NODES;
        participants[owner_n].push_back(p);
        participants[replica_node].push_back(p);

    }

    // std::cout << "Successfully stored owner and replica nodes in participants\n";

    unordered_map<int, int> socks; // for opening connection to each participant

    for (auto&[node, _] : participants){
        int sock = connect_to(nodes[node].ip, nodes[node].port);
        if (sock<0){
            return false;
        }
        socks[node] = sock;//store
    }

    // std::cout << "Made connections to all participants! from Node " << to_string(self_id) << "\n";

    //now that connection to each is open, send prepare w/ kv pairs to each one:

    for (auto &[node, sock] : socks){
        string msg = to_string(txn_id);
        for (auto &pair : participants[node]){ //only send each participant their pairs, not everything
            msg += " " + to_string(pair.first) + " " + pair.second; 
        }
        send_line(sock, "PREPARE " + msg + "\n");
    }

    // std::cout << "Sent PREPARE to all participants! from Node " << to_string(self_id) << "\n";

    //wait and collect their responses
    bool all_commit = true;
    unordered_map<int, string> replies;

    for (auto &[node, sock] : socks){
        string response;
        if (!recv_line(sock, response)){
            all_commit = false; //didnt receive reply from someone
            //todo: wait ?
        }
        replies[node] = response;
        if (response.rfind("VOTE_COMMIT", 0) != 0){
            all_commit = false; //if one says no, then abort
        }
    }

    // std::cout << "Collected all responses! Node " << to_string(self_id) << "\n";

    if (all_commit){
        // std::cout << "this put_many is a commit!\n";
    }
    else{
        // std::cout << "this put_many is an ABORT!\n";
    }

    //send commit / abort to all
    for (auto &[node, sock] : socks){
        if (all_commit){
            send_line(sock, "COMMIT "+ to_string(txn_id) + "\n");
        }
        else{
            send_line(sock, "ABORT "  +  to_string(txn_id) + "\n");
            //todo: return false? but also need to close socks
        }
    }

    // std::cout << "Sent commit/abort to all participants! from Node " << to_string(self_id) << "\n";

    //close socks
    for (auto &[node, sock] : socks){
        close(sock);
    }

    //todo: do local writes here now if committed?
    if (all_commit){
        //do local writes;
    }


    return all_commit; //return true or false based on if we could commit
    
}

void parse_put(const std::string& line, int& key, std::string& value)
{
    // Expected format: "PUT <key> <value>\n"
    size_t first = line.find(' ');
    size_t second = line.find(' ', first + 1);

    key = std::stoi(line.substr(first + 1, second - first - 1));
    value = line.substr(second + 1);

    if (!value.empty() && value.back() == '\n')
        value.pop_back();
}
int parse_key(const std::string& line)
{
    // Expected format: "GET <key>\n"
    size_t space = line.find(' ');
    if (space == std::string::npos)
        return -1; // malformed

    int key = std::stoi(line.substr(space + 1));
    return key;
}

int parse_txn_id(const std::string& line){
    //exp format: "PREPARE 123 10 a 11 b 12 c"

    size_t space1 = line.find(' ');
    if (space1 == std::string::npos){
        return -1;
    }
    size_t space2 = line.find(' ', space1+1);

    std::string txn_string = line.substr(space1 + 1,
    (space2 == std::string::npos) ? std::string::npos : space2 - space1 - 1);

    return std::stoi(txn_string);
}

vector<pair<int, string>> parse_put_many(const std::string& line){
    //exp format: "PREPARE 123 10 a 11 b 12 c"

    //chatgpt. todo: test

    vector<pair<int, string>> kv_pairs;
    std::istringstream iss(line);

    string cmd;
    int txn_id;
    iss >> cmd >> txn_id;  // first two tokens

    int key;
    string value;

    while (iss >> key >> value) {
        kv_pairs.emplace_back(key, value);
    }

    return kv_pairs;

}

std::optional<std::string> dht_get(int key)
{
    int owner = owner_node(key);

    if (owner == self_id) {
        acquire_read(key);
        auto* val = ht_get(local_table, key);
        std::optional<std::string> result =
            val ? std::optional(*val) : std::nullopt;
        release_read(key);
        return result;
    }

    int sock = connect_to(nodes[owner].ip, nodes[owner].port);
    if (sock < 0) return std::nullopt;

    send_line(sock, "GET " + std::to_string(key) + "\n");

    std::string response;
    recv_line(sock, response);
    close(sock);

    if (response.rfind("OK ", 0) == 0)
        return response.substr(3);

    return std::nullopt;
}

void unlock_all(std::vector<int> locked_keys){
    for (auto key : locked_keys){
        int owner = owner_node(key);
        if (owner == self_id){
            release_write(key);
        }
    }
}

void handle_prepare(int sock, std::string line, int txn_id){

    vector<pair<int, string>> vec = parse_put_many(line);
    int parsed_txn_id = parse_txn_id(line); 

    sort(vec.begin(), vec.end());//so no deadlock    
    
    //init transaction state
    TxnState txn;
    txn.txn_id = parsed_txn_id;
    for (auto p : vec){
        txn.writes.emplace(p.first, p.second);
    }
    // txn.writes = vec; //so don't need to parse kvs from line, get from txns 
    txn.status = TxnStatus::PREPARING; //maybe don't need to record if its preparing

    { //lock txn table and add txn
        lock_guard<std::mutex> lock(txn_table_mtx);
        transactions[parsed_txn_id] = txn;
    }

    //locking all keys from msg
    // vector<int> locked_keys;
    bool all_locked = true;

    for (auto pair : vec) {
        int key = pair.first;
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            if (acquire_write_timeout(key, 100) == true){ //true means it timed out
                all_locked = false;
                break; //dont need to try to lock others, one fail = all fail
            }
            else{
                txn.locked_keys.push_back(key); //so we know who to unlock in case of abort
            }
        }
    }

        //now if locking was successful, send back ok
    if (all_locked == true){
        lock_guard<std::mutex> lock(txn_table_mtx);
        transactions[txn_id].status = TxnStatus::PREPARED; //update status
        send_line(sock, "VOTE_COMMIT " + to_string(txn.txn_id) + "\n");
    }
    else{
        unlock_all(txn.locked_keys); 
        {
            std::lock_guard<std::mutex> lock(txn_table_mtx);
            transactions[txn_id].status = TxnStatus::ABORTED; //new status
        }
        send_line(sock, "VOTE_ABORT " + to_string(txn_id) + "\n");
        //todo: need to actually remove it from map with transactions.erase(txn_id). when?
    }
}


void handle_commit(int sock, int txn_id){
    
    lock_guard<mutex> lock(txn_table_mtx);

    auto it = transactions.find(txn_id);
    if (it == transactions.end()){
        printf("Unknown transaction\n");
        return;
    }
    
    TxnState &txn = it->second;
    unordered_map<int, string> map_writes = txn.writes; //get writes from the txn node

    if (txn.status == TxnStatus::COMMITTED){
        return; //redundant
    }
    if (txn.status != TxnStatus::PREPARED){
        return; // todo: or maybe check if aborted and then delete it from map??
    }
    for (int key : txn.locked_keys) {
        // int owner = owner_node(pair.first);
        // if (owner+1 == self_id){ //todo: not just +1
        //must still check ownership even though commt only sent to participants, non participant node could receive due to bug or network retry?
        //and not every participant has every key, different participants for dif keys
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            ht_put(local_table, key, txn.writes[key]); //key, val
            release_write(key);
        }
        // }
    }

    txn.status = TxnStatus::COMMITTED;
    transactions.erase(txn_id);
}

void handle_abort(int sock, int txn_id){
    //note: don't actually need string line as an input, since we get all info from transactions with the id

    std::lock_guard<std::mutex> lock(txn_table_mtx);

    auto it = transactions.find(txn_id);
    if (it == transactions.end())
        return;

    TxnState &txn = it->second;

    if (txn.status == TxnStatus::ABORTED){
        return; //we already set it
    }
    
    for (int key: txn.locked_keys){
        //todo: maaaybe also check ownership? unsure. seems if everything works correctly, non-participant nodes will never receive abort or commit msgs for foreign keys
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            release_write(key);
        }
        
    }
    //todo: lock txn? already done above oops
    txn.status = TxnStatus::ABORTED; //when should we actually remove it?
}

//server-side handling
void handle_client(int sock)
{
    std::string line;
    if (!recv_line(sock, line)) {
        close(sock);
        return;
    }


    if (line == "BARRIER READY\n") {
        if (self_id != 0) {
            send_line(sock, "ERROR not coordinator\n");
            close(sock);
            return;
        }

        std::unique_lock<mutex> lock(barrier_mtx);
        barrier_ready++;
        if(barrier_ready == NUM_NODES - 1){
            barrier_cv.notify_all();
        }
        else{
            barrier_cv.wait(lock, [] {
                return barrier_ready >= NUM_NODES - 1;
            });
        }
        // {
        //     std::lock_guard<std::mutex> lock(barrier_mtx);
        //     barrier_ready++;
        //     if (barrier_ready == NUM_NODES - 1)
        //         send_go = true;
        // }

        // Waits until all arrive
        // while (true) { //todo: change this to condition variable, not spin loop. burns cpu
        //     std::lock_guard<std::mutex> lock(barrier_mtx);
        //     if (barrier_ready >= NUM_NODES - 1)
        //         break;
        // }

        send_line(sock, "BARRIER GO\n");
        close(sock);
        return;
    }

    if (line.rfind("PUT", 0) == 0) {
        int key;
        std::string value;
        parse_put(line, key, value);

        if (owner_node(key) != self_id) {
            send_line(sock, "FAIL\n");
            close(sock);
            return;
        }

        acquire_write(key);
        bool ok = ht_put(local_table, key, value);
        release_write(key);

        send_line(sock, ok ? "OK\n" : "FAIL\n");
    }


    else if (line.rfind("GET", 0) == 0)
 {
        int key = parse_key(line);

        if (owner_node(key) != self_id) {
            send_line(sock, "NULL\n");
            close(sock);
            return;
        }

        acquire_read(key);
        auto* val = ht_get(local_table, key);
        release_read(key);

        if (val)
            send_line(sock, "OK " + *val + "\n");
        else
            send_line(sock, "NULL\n");
    }

    else if (line.rfind("PREPARE", 0) == 0){
        int txn_id = parse_txn_id(line);
        handle_prepare(sock, line, txn_id);
    }

    else if (line.rfind("COMMIT", 0) == 0){
        int txn_id = parse_txn_id(line);
        handle_commit(sock, txn_id);
    }
    else if (line.rfind("ABORT", 0) == 0){
        int txn_id = parse_txn_id(line);
        handle_abort(sock, txn_id);
        transactions.erase(txn_id);
    }

    close(sock);
}

// int server_fd = -1;

void server_loop()
{
    int server_fd = start_server(nodes[self_id].port);
    if (server_fd < 0) {
        std::cerr << "[Node " << self_id << "] Failed to start server\n" << std::flush;
        return;
    }
    std::cout << "[Node " << self_id << "] Server TEST TEST listening on port " << nodes[self_id].port << std::endl;
    std::cout.flush();

    // server_fd = start_server(nodes[self_id].port+1); had this here twice lol so nodes were always taken

    while (true) {
        // std::cout << "[Node" << self_id << "] Entered the while(true) loop in serverloop\n";
        std::cout.flush();
        int client = accept_client(server_fd);
        if (client == -1){
            std::cout << "[Node" << self_id << "] Couldn't accept client\n";
            std::cout.flush();
        }
        // std::cout << "[Node" << self_id << "] Accepted barrier connection\n";
        std::cout.flush();
        if (client >= 0)
            std::thread(handle_client, client).detach();
    }
}

void dht_printall() {
    std::cout << "[Node " << self_id << "] Local table contents:\n";
    print_table(local_table);  // ht funct
}
