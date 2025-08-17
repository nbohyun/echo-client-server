#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <algorithm>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int MAX_CLIENTS = 5; // 동시 접속 가능한 클라이언트 수 5
constexpr int BUF_SIZE = 1024;

bool echo_flag = false;
bool broadcast_flag = false;

std::vector<int> clients;
std::mutex mtx;

void send_message(const std::string& msg, int sender) {
    std::lock_guard<std::mutex> lock(mtx);
    for (int sock : clients) {
        if (broadcast_flag || sock == sender) {
            ::send(sock, msg.c_str(), msg.size(), 0);
        }
    }
}

void client_handler(int sock) {
    char buf[BUF_SIZE];
    while (true) {
        int len = ::recv(sock, buf, sizeof(buf)-1, 0);
        if (len <= 0) break;

        buf[len] = '\0';
        std::string msg(buf);

        std::cout << msg << std::endl;

        if (echo_flag || broadcast_flag) {
            send_message(msg, sock);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        clients.erase(std::remove(clients.begin(), clients.end(), sock), clients.end());
    }
    ::close(sock);
    std::cout << "Disconnected a client" << std::endl; // sock 번호 대신 단순 메시지
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [-e [-b]]" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "-e") echo_flag = true;
        if (std::string(argv[i]) == "-b") broadcast_flag = true;
    }

    int serv_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_adr{};
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = INADDR_ANY;
    serv_adr.sin_port = htons(port);

    if (::bind(serv_sock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        perror("bind");
        return 1;
    }
    if (::listen(serv_sock, MAX_CLIENTS) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "Server running... port " << port << std::endl;

    while (true) {
        sockaddr_in clnt_adr{};
        socklen_t clnt_sz = sizeof(clnt_adr);
        int clnt_sock = ::accept(serv_sock, (sockaddr*)&clnt_adr, &clnt_sz);

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (clients.size() >= MAX_CLIENTS) {
                std::string reject_msg = "Server full: cannot accept more than 5 clients.\n";
                ::send(clnt_sock, reject_msg.c_str(), reject_msg.size(), 0);
                std::cout << "Max clients reached, rejecting new client" << std::endl;
                ::close(clnt_sock);
                continue;
            }
            clients.push_back(clnt_sock);
        }

        std::cout << "Connected a new client" << std::endl;
        std::thread(client_handler, clnt_sock).detach();
    }
}
