#pragma once
#include <string>
#include "config.h"

// extern NodeInfo nodes[NUM_NODES];
void load_config();

int start_server(int port);
int accept_client(int server_fd);

int connect_to(const std::string& ip, int port);

bool send_line(int sock, const std::string& line);
bool recv_line(int sock, std::string& line);
