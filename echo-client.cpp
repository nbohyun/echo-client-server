#include <iostream>
#include <thread>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int BUF_SIZE = 1024;

void recv_thread(int sock) {
    char buf[BUF_SIZE];
    std::string buffer;

    while (true) {
        int len = ::recv(sock, buf, sizeof(buf)-1, 0);
        if (len <= 0) break;

        buf[len] = '\0';
        buffer += buf;

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos+1);
            std::cout << "From server: " << line << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_adr{};
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(std::stoi(argv[2]));

    if (::connect(sock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        perror("connect");
        return 1;
    }

    std::thread(recv_thread, sock).detach();

    std::string msg;
    while (true) {
        if (!std::getline(std::cin, msg)) break;
        msg += "\n";
        ::send(sock, msg.c_str(), msg.size(), 0);
    }

    ::close(sock);
    return 0;
}
