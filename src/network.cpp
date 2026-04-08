#include <string>
#include "network.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

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

    // fcntl(fd, F_SETFL, O_NONBLOCK);//set socket nonblocking for retry loop in dist barrier

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        // if (errno != EINPROGRESS) { // connection failed, return -1
            close(fd);
            return -1;
        // }
    }

    // fd_set wfds;
    // FD_ZERO(&wfds);
    // FD_SET(fd, &wfds);
    // timeval tv{0, 500000}; //.5 sec
    // if (select(fd + 1, nullptr, &wfds, nullptr, &tv) <= 0) {
    //     close(fd);
    //     return -1;
    // }

    return fd;

}

bool send_line(int sock, const std::string& line) {
    ssize_t sent = send(sock, line.c_str(), line.size(), MSG_NOSIGNAL); // prevent SIGPIPE
    if (sent == -1) {
        perror("send failed");
        return false;
    }
    return sent == (ssize_t)line.size(); // true if entire line was sent
}

bool recv_line(int sock, std::string& line){
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    if ( n<= 0){
        return false;
    }
    buf[n] = 0;
    line = buf;
    return true;
}


//chatgpt
bool recv_line_timeout(int sock, std::string& line, int timeout_sec) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    int ret = select(sock + 1, &fds, nullptr, nullptr, &tv);

    if (ret == 0) {
        // timeout
        return false;
    } else if (ret < 0) {
        // error
        return false;
    }

    // socket is ready to read
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        return false;
    }

    buf[n] = '\0';
    line = buf;
    return true;
}


void load_config(){
    nodes[0] = {"128.180.120.95", 6005}; //io: 128.180.120.77, neptune: 128.180.120.95
    nodes[1] = {"128.180.120.73", 6005}; //eris: 128.180.120.73
    nodes[2] = {"128.180.120.86", 6005}; //saturn . puck: 128.180.120.86
    nodes[3] = {"128.180.120.76", 6005}; // callisto: 128.180.120.67 . iapetus: 128.180.120.76
    // nodes[3] = {"127.0.0.1", 5003};
}

