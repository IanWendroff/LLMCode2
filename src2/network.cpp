#include <string>
#include "network.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cerrno>
#include <ctime>

int self_id;
NodeInfo nodes[NUM_NODES];
int start_server(int port){
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { //allow ports to be reused
        perror("setsockopt");
        exit(1);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    std::cout << "[Node " << self_id << "] Trying to bind server on port " 
          << nodes[self_id].port << std::endl;
    std::cout.flush();


    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { //make sure binding works
        perror("bind");
        exit(1);
    }

    if (listen(fd, 16) < 0) { //make sure listening works
        perror("listen");
        exit(1);
    }
    return fd;

}
int accept_client(int server_fd){
    return accept(server_fd, nullptr, nullptr);
}

int connect_to(const std::string& ip, int port){ //client side
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            return -1;
    }
    return fd;

}

bool send_line(int sock, const std::string& line) {
    size_t total_sent = 0;
    while (total_sent < line.size()) {
        ssize_t sent = send(
            sock,
            line.c_str() + total_sent,
            line.size() - total_sent,
            MSG_NOSIGNAL
        );

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send failed");
            return false;
        }
        if (sent == 0) {
            return false;
        }

        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

bool recv_line(int sock, std::string& line){
    line.clear();

    while (true) {
        char ch = 0;
        ssize_t n = recv(sock, &ch, 1, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return !line.empty();
        }

        line.push_back(ch);
        if (ch == '\n') {
            return true;
        }
    }
}


//chatgpt
bool recv_line_timeout(int sock, std::string& line, int timeout_sec) {
    line.clear();
    time_t deadline = time(nullptr) + timeout_sec;

    while (true) {
        time_t now = time(nullptr);
        if (now >= deadline) {
            return false;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        struct timeval tv;
        tv.tv_sec = static_cast<long>(deadline - now);
        tv.tv_usec = 0;

        int ret = select(sock + 1, &fds, nullptr, nullptr, &tv);
        if (ret == 0) {
            return false;
        }
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        char ch = 0;
        ssize_t n = recv(sock, &ch, 1, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return !line.empty();
        }

        line.push_back(ch);
        if (ch == '\n') {
            return true;
        }
    }
}


void load_config(){
    // Update these to the 4 machines/port you use on Sunlab, per readme.md.
    // Using 4 nodes as required by NUM_NODES.
    nodes[0] = {"128.180.120.65", 4040};
    nodes[1] = {"128.180.120.66", 4040};
    nodes[2] = {"128.180.120.77", 4040};
    nodes[3] = {"128.180.120.68", 4040};

    // Spare machine from your list (swap in if one above is busy): 128.180.120.69
}