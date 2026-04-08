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
#include <map>
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
    uint64_t txn_id;   
    // vector<pair<int, string>> writes; //kv pairs we're trying to write
    unordered_map<int, std::string> writes;
    TxnStatus status;
    vector<int> locked_keys;//keys that are locked so we know who to unlock if fail
    chrono::steady_clock::time_point start_time; //timer for reaper
};

unordered_map<uint64_t, TxnState> transactions;
mutex txn_table_mtx;

//condition variable for barrier loop
std::condition_variable barrier_cv;
/*condition variable is a synchronization primitive that is used to 
notify the other threads in a multithreading environment that the 
shared resource is free */

// std::mutex barrier_mtx;

static HashTable<std::string>* local_table;  //of kv pairs on each node
// static std::unordered_map<int, KeyLock> key_locks;
// static std::mutex key_locks_mutex;






KeyLock& get_lock(int key)
{
    std::lock_guard<std::mutex> g(key_locks_mutex); //lock guard so releases when out of scope
    auto it = key_locks.find(key);
    if (it == key_locks.end()){
        key_locks[key] = std::make_unique<KeyLock>();
        return *key_locks[key];
    }
    return *(it->second); // creates if missing
    //returns a poiunter to the KeyLock 
}





//stop transactions from hanging by reaping them if too long
void transaction_reaper() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); //runs every 5 seconds
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(txn_table_mtx);
        for (auto it = transactions.begin(); it != transactions.end(); ) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.start_time);
            
            if ((it->second.status == TxnStatus::PREPARED || it->second.status == TxnStatus::PREPARING) && duration.count() > 8) {
                uint64_t id = it->first;
                std::cout << "[Reaper] Txn " << id << " timed out. Forcing abort. Reaping: \n";
                
                // Release locks for this txn
                for (int key : it->second.locked_keys) {
                    release_write(key);
                    std::cout <<  key << ", ";
                }
                cout << "\n";
                it->second.status = TxnStatus::ABORTED;
                it = transactions.erase(it); // Remove and get next iterator
            } else {
                ++it;
            }
        }
    }
}

void dht_init(int node_id)
{
    self_id = node_id;
    local_table = create_table<std::string>(CAPACITY);

    thread(transaction_reaper).detach(); //start reaper 
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
    return hash_function(key) % NUM_NODES;
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
    
    uint64_t txn_id = generate_txn_id(self_id); 
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
        if (replica_node != owner_n) { //so no duplicates
            participants[replica_node].push_back(p);
        }
    }

    // std::cout << "Successfully stored owner and replica nodes in participants\n";

    unordered_map<int, int> socks; // for opening connection to each participant
    

    for (auto&[node, _] : participants){
        if (node == self_id) continue; //dont open socket to self bc could block if it sends prepare then calls recv and waits
        int sock = connect_to(nodes[node].ip, nodes[node].port);
        if (sock<0){
            return false;
        }
        socks[node] = sock;//store
    }

    map<int, int> ordered_socks(socks.begin(), socks.end()); //order by node so no deadlock
    //so nodes contacted in consistent order so no cross-nose deadlock (eg node A is the replica for 2 dif owners and creates deadlock on nodes)


    // std::cout << "Made connections to all participants! from Node " << to_string(self_id) << "\n";

    //lock local keys if coordinator owns some keys
    bool all_commit = true;
    bool all_local_locked = true;
    if (participants.count(self_id)){
        TxnState txn;
        txn.txn_id = txn_id;
        txn.status = TxnStatus::PREPARING;
        txn.start_time = std::chrono::steady_clock::now();

        for (auto &p : participants[self_id]){ //for each pair in participants
            if (acquire_write_timeout(p.first, 1000)){ //todo: revert. also todo: doesnt the lock go out of scope or something? or wait no it doesnt bc its unique lock
                all_local_locked = false; //timeout occurred
                break;
            }
            std::cout << "Acquired lock on coordinator node " << self_id << " for pair [" << p.first << ", " << p.second << "]\n";

            txn.locked_keys.push_back(p.first); //why is this not accurately storing them. todo add print statements of locked keys, run with small number of ops
            txn.writes[p.first] = p.second;
        }
        std::cout << "Node " << self_id << ", transaction " << txn.txn_id << " has locked keys:";
        for (auto & p : txn.locked_keys){
            std::cout << ", " << p;
        }
        cout << "\n";
        
        lock_guard<mutex> lock(txn_table_mtx); //moved here so no global lock while we do timeout locks
        if (all_local_locked){
            txn.status = TxnStatus::PREPARED;
            transactions[txn_id] = std::move(txn); //todo: ??
        }
        else{
            //failed to lock, release what we have and abort
            std::cout << "Aborting txn " << txn.txn_id << " bc coordinator node " << self_id << " failed to lock all keys\n";
            if (!txn.locked_keys.empty()){
            for (int k : txn.locked_keys){
                std::cout << "Releasing lock on coordinator node " << self_id << " for key [" << k << "]\n";
                release_write(k);
            }
        }
            all_commit = false;
        }
    }

    if (all_commit){ //added this on 3/18
        //now that connection to each is open, send prepare w/ kv pairs to each one: 
        for (auto &[node, sock] : ordered_socks){
            string msg = to_string(txn_id);
            for (auto &pair : participants[node]){ //only send each participant their pairs, not everything
                msg += " " + to_string(pair.first) + " " + pair.second; 
            }
            send_line(sock, "PREPARE " + msg + "\n");
        }
    

        // std::cout << "Sent PREPARE to all participants! from Node " << to_string(self_id) << "\n";

        //wait and collect their responses
        
        unordered_map<int, string> replies; //just for debugging

        for (auto &[node, sock] : ordered_socks){
            string response;
            if (!recv_line_timeout(sock, response, 5)){ //wait 2 seconds for response.. should i wait longer?
                all_commit = false; //didnt receive reply from someone
                continue; //so we don't try to parse the response
                //todo: wait ?
            }
            replies[node] = response;
            if (response.rfind("VOTE_COMMIT", 0) != 0){ //if there is something thats NOT a a commit
                all_commit = false; //if one says no, then abort
            }
        // }

    }   

    // std::cout << "Collected all responses! Node " << to_string(self_id) << "\n";

    // if (all_commit){
    //     std::cout << "this put_many is a commit!\n";
    // }
    // else{
    //     std::cout << "this put_many is an ABORT!\n";
    // }

    //send commit / abort to all
    for (auto &[node, sock] : ordered_socks){
        if (all_commit){
            cout << "SENDING commit to participant node " << node << " for txn "<< txn_id << "\n";
            if (!send_line(sock, "COMMIT "+ to_string(txn_id) + "\n")){
                cout << "ERROR: SENDING COMMIT to node " << node << " FAILED\n";
            }
            //lock, put, unlock if 0 in participants
        }
        else{
            cout << "SENDING abort to participant node " << node << " for txn "<< txn_id << "\n";
            if (!send_line(sock, "ABORT "  +  to_string(txn_id) + "\n")){
                cout << "ERROR: SENDING ABORT to node " << node << " FAILED\n";
            }
            //todo: return false? but also need to close socks
        }
    }

    // std::cout << "Sent commit/abort to all participants! from Node " << to_string(self_id) << "\n";
    
    //local puts
    if (participants.count(self_id)) {
        TxnState txn;
        {
            std::lock_guard<std::mutex> lock(txn_table_mtx);
            txn = transactions[txn_id];
        }
        
        if (all_commit && all_local_locked && transactions.count(txn_id)) {
            // Apply writes and release

            if (!txn.locked_keys.empty()){
            for (int key : txn.locked_keys) {
                std::cout << "Putting on coordinator node " << self_id << " for key [" << key << "]\n";
                if (!ht_put(local_table, key, txn.writes[key])){
                    all_commit = false; //locking ok but actually committing failed - key already in table
                }
            }
            for (int key : txn.locked_keys) {
                std::cout << "Releasing lock on coordinator node " << self_id << " for key [" << key << "]\n";
                release_write(key);
            }
        }

        } else {
            // Just release
            if (transactions.count(txn_id)) {
                if (!txn.locked_keys.empty()){
                for (int key : txn.locked_keys) {
                    std::cout << "NOT putting on coordinator node " << self_id << " for key [" << key << "], just releasing lock\n";
                    release_write(key);
                }
            }
            }
        }
        transactions.erase(txn_id); // remove
    }

    //close socks
    for (auto &[node, sock] : ordered_socks){
        close(sock);
    }
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

uint64_t parse_txn_id(const std::string& line){
    //exp format: "PREPARE 123 10 a 11 b 12 c"

    size_t space1 = line.find(' ');
    if (space1 == std::string::npos){
        return -1;
    }
    size_t space2 = line.find(' ', space1+1);
    // if (space2 == std::string::npos){ //todo: comment back in
    //     return -1;
    // }

    std::string txn_string = line.substr(space1 + 1,
    (space2 == std::string::npos) ? std::string::npos : space2 - space1 - 1);

    return std::stoull(txn_string);
}

vector<pair<int, string>> parse_put_many(const std::string& line){
    //exp format: "PREPARE 123 10 a 11 b 12 c"

    vector<pair<int, string>> kv_pairs;
    std::istringstream iss(line);

    string cmd;
    uint64_t txn_id;
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
    int replica = (owner + 1) % NUM_NODES;

    if ((owner == self_id) || (replica == self_id)) {
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

void handle_prepare(int sock, std::string line, uint64_t txn_id){

    vector<pair<int, string>> vec = parse_put_many(line);
    // int parsed_txn_id = parse_txn_id(line); 

    sort(vec.begin(), vec.end());//sort by keys so no deadlock    
    
    //init transaction state
    TxnState txn;
    txn.txn_id = txn_id;
    for (auto p : vec){
        txn.writes.emplace(p.first, p.second);
    }
    // txn.writes = vec; //so don't need to parse kvs from line, get from txns 
    txn.status = TxnStatus::PREPARING; //maybe don't need to record if its preparing
    txn.start_time = chrono::steady_clock::now();

    //locking all keys from msg
    // vector<int> locked_keys;
    bool all_locked = true;

    for (auto pair : vec) {
        int key = pair.first;
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            if (acquire_write_timeout(key, 1000) == true){ //true means it timed out
                all_locked = false;
                break; //dont need to try to lock others, one fail = all fail
            }
            std::cout << "Acquired lock on participant node " << self_id << " for pair [" << pair.first << ", " << pair.second << "]\n";
            // if (true){ //todo: revert back to writer lock
            //     int x = 1;
            // }
            // else{
                // std::lock_guard<std::mutex> lock(txn_table_mtx);
                txn.locked_keys.push_back(key); //so we know who to unlock in case of abort
                //changed the above to only do local locked_keys, not global. 3/18
            // }
            std::cout << "Node " << self_id << ", transaction " << txn.txn_id << " has locked keys: ";
            if (!txn.locked_keys.empty()){
            for (auto & p : txn.locked_keys){
                std::cout << p<< ", ";
            }
        }
            cout << "\n";
        }
    }

        //now if locking was successful, send back ok
    if (all_locked == true){
        {
            lock_guard<std::mutex> lock(txn_table_mtx);
            auto it = transactions.find(txn_id);
            if (it == transactions.end()) //only put if its not already in there
                transactions[txn_id] = txn;
            transactions[txn_id].locked_keys = std::move(txn.locked_keys);  //update transactions to reflect the keys we just locked
            transactions[txn_id].status = TxnStatus::PREPARED; //update status
        }
        send_line(sock, "VOTE_COMMIT " + to_string(txn.txn_id) + "\n");
    }
    else{
        std::cout << "All-locked unsuccessful on participant node " << self_id <<", txn_id " << txn.txn_id << "\n";
        {
            std::lock_guard<std::mutex> lock(txn_table_mtx);
            transactions[txn_id].status = TxnStatus::ABORTED; //new status
            transactions[txn_id].locked_keys = std::move(txn.locked_keys);
            // transactions[txn_id].locked_keys.clear();
            // transactions.erase(txn_id);
        }
        // for (int key : txn.locked_keys){
        //     std::cout << "key [" << key << "] releasing lock\n";
        //     release_write(key);
        // }
        send_line(sock, "VOTE_ABORT " + to_string(txn_id) + "\n");
        //actually remove it from map with transactions.erase(txn_id). after call to handle_abort
    }
}


void handle_commit(int sock, uint64_t txn_id){
    
    TxnState txn;
    {
        lock_guard<mutex> lock(txn_table_mtx); //todo: maybe bad to hold this lock so long
        // txn = transactions[txn_id];
        auto it = transactions.find(txn_id);
        if (it == transactions.end()){
            std::cout << "[Node " << self_id << "] Commit failed: Unknown Txn " << txn_id << "\n";
            return;
        }
        txn = it->second;
    }

    
    
    // TxnState &txn = it->second;
    // unordered_map<int, string> map_writes = txn.writes; //get writes from the txn node

    if (txn.status == TxnStatus::COMMITTED){
        return; //redundant
    }
    if (txn.status != TxnStatus::PREPARED){
        return; // todo: or maybe check if aborted and then delete it from map??
    }
    if (!txn.locked_keys.empty()){
    for (int key : txn.locked_keys) {
        // int owner = owner_node(pair.first);
        // if (owner+1 == self_id){ //todo: not just +1
        //must still check ownership even though commt only sent to participants, non participant node could receive due to bug or network retry?
        //and not every participant has every key, different participants for dif keys
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            ht_put(local_table, key, txn.writes[key]); //key, val
            std::cout << "Putting on participant node " << self_id << " for key [" << key << "]\n";                
        }
        // }
    }
}
if (!txn.locked_keys.empty()){
    for (int key : txn.locked_keys) {//trying a separate loop for releasing
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            std::cout << "Releasing lock on participant node " << self_id << " for key [" << key << "]\n";
            release_write(key);
        }
        // }
    }
}
    {
        lock_guard<mutex> lock(txn_table_mtx);
        transactions[txn_id].locked_keys.clear(); //unecessary
    }
    
    // txn.status = TxnStatus::COMMITTED;
    // transactions.erase(txn_id); //shouldn't need to clear locked_keys bc we erase the entire thing
}

void handle_abort(int sock, uint64_t txn_id){
    //note: don't actually need string line as an input, since we get all info from transactions with the id

    cout << "entering handle_abort for node " << self_id << "\n";
    std::vector<int> keys;
    { //scope for the mutex
    std::lock_guard<std::mutex> lock(txn_table_mtx);

    auto it = transactions.find(txn_id);
    if (it == transactions.end()){
        std::cout << "[Node " << self_id << "] Abort failed: Unknown Txn " << txn_id << "\n";
        return;
    }

    TxnState &txn = it->second;

    if (txn.status == TxnStatus::ABORTED){
        return; //we already set it
    }
    txn.status = TxnStatus::ABORTED;
    keys = txn.locked_keys;
}
    
    for (int key: keys){
        //todo: maaaybe also check ownership? unsure. seems if everything works correctly, non-participant nodes will never receive abort or commit msgs for foreign keys
        if ((owner_node(key) == self_id) || (((owner_node(key)+1) %NUM_NODES) == self_id)){
            std::cout << "Aborted so Releasing lock on participant node " << self_id << " for key [" << key << "]\n";
            release_write(key);
        }
        
    }
    //todo: lock txn? already done above oops
    // txn.status = TxnStatus::ABORTED; //when should we actually remove it?
    // transactions.erase(txn_id);
}

//server-side handling
void handle_client(int sock)
{
    std::string line;
    // if (!recv_line(sock, line)) {
    //     close(sock);
    //     return;
    // }
    while (true){
        if (!recv_line(sock, line)){
            break; //try again
        }

    

    if (line == "BARRIER READY\n") {
        if (self_id != 0) { 
            send_line(sock, "ERROR not coordinator\n");
            close(sock);
            return;
            // break; //closes connection
        }

        std::unique_lock<mutex> lock(barrier_mtx);
        barrier_ready++;
        if(barrier_ready == NUM_NODES - 1){
            barrier_cv.notify_all();
            // barrier_ready = 0; //reset for next barrier
        }
        else{
            barrier_cv.wait(lock, [] {
                return barrier_ready >= NUM_NODES - 1;
            });
        }

        send_line(sock, "BARRIER GO\n");
        close(sock);
        return;
        // break; //close this connection after barrier
    }


    else if (line.rfind("GET", 0) == 0)
 {
        int key = parse_key(line);

        if (owner_node(key) != self_id) {
            send_line(sock, "NULL\n");
            // close(sock);
            // return;
            continue;
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
        uint64_t txn_id = parse_txn_id(line);
        cout << "server is PREPARING and calling handle_prepare with txn " << txn_id << "\n";
        handle_prepare(sock, line, txn_id);
    }

    else if (line.rfind("COMMIT", 0) == 0){
        uint64_t txn_id = parse_txn_id(line);
        cout << "server is COMMITTING and calling handle_commit with txn " << txn_id << "\n";
        handle_commit(sock, txn_id);

        std::lock_guard<std::mutex> lock(txn_table_mtx);
        transactions.erase(txn_id);
    }
    else if (line.rfind("ABORT", 0) == 0){
        uint64_t txn_id = parse_txn_id(line);
        cout << "server is ABORTING and calling handle_abort with txn " << txn_id << "\n";
        handle_abort(sock, txn_id);

        std::lock_guard<std::mutex> lock(txn_table_mtx);
        transactions.erase(txn_id);
        cout << "Abort for txn " << txn_id << " complete\n";
    }
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
