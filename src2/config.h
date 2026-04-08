#pragma once
#include <string>

#define NUM_NODES 4

struct NodeInfo {
    std::string ip;
    int port;
};
extern int self_id;  
extern NodeInfo nodes[NUM_NODES];