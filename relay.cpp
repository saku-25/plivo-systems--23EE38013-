#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdlib>

#pragma pack(push, 1)
struct InternalPacket {
    uint32_t seq;       // Big Endian
    uint8_t type;       // 0 = DATA, 1 = ACK
    uint64_t tx_time;   // Big Endian (microseconds)
    uint8_t payload[160];
};
#pragma pack(pop)

// Tweakable constants for ultra‑low latency
constexpr int EPOLL_MAX_EVENTS = 64;
constexpr int BUSY_POLL_US    = 0;   // set to e.g. 50 (microseconds) for busy polling

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void add_epoll(int epfd, int fd, uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int main() {
    // Sockets that the relay will listen on
    int sock_from_sender   = socket(AF_INET, SOCK_DGRAM, 0);
    int sock_from_receiver = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
    setsockopt(sock_from_sender,   SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(sock_from_receiver, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // Increase socket receive buffers for fewer drops under load
    int rcvbuf = 4 * 1024 * 1024; // 4 MB
    setsockopt(sock_from_sender,   SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    setsockopt(sock_from_receiver, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    // Bind: listen for data from sender on 47001, for ACKs from receiver on 47003
    sockaddr_in addr_sender{};
    addr_sender.sin_family = AF_INET;
    addr_sender.sin_addr.s_addr = INADDR_ANY;
    addr_sender.sin_port = htons(47001);
    bind(sock_from_sender, (sockaddr*)&addr_sender, sizeof(addr_sender));

    sockaddr_in addr_receiver{};
    addr_receiver.sin_family = AF_INET;
    addr_receiver.sin_addr.s_addr = INADDR_ANY;
    addr_receiver.sin_port = htons(47003);
    bind(sock_from_receiver, (sockaddr*)&addr_receiver, sizeof(addr_receiver));

    // Fixed destinations
    sockaddr_in dest_receiver{};   // forward DATA to receiver on 47002
    dest_receiver.sin_family = AF_INET;
    dest_receiver.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_receiver.sin_port = htons(47002);

    sockaddr_in dest_sender{};     // forward ACK to sender on 47004
    dest_sender.sin_family = AF_INET;
    dest_sender.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_sender.sin_port = htons(47004);

    // Set sockets to non‑blocking for edge‑triggered epoll
    set_nonblocking(sock_from_sender);
    set_nonblocking(sock_from_receiver);

    // Create epoll instance (edge‑triggered for minimal wake‑ups)
    int epfd = epoll_create1(0);
    add_epoll(epfd, sock_from_sender,   EPOLLIN | EPOLLET);
    add_epoll(epfd, sock_from_receiver, EPOLLIN | EPOLLET);

    epoll_event events[EPOLL_MAX_EVENTS];
    InternalPacket pkt;

    std::cout << "[Relay] Ultra‑low‑latency relay running" << std::endl;

    while (true) {
        // Wait for events (busy polling if BUSY_POLL_US > 0)
        int nfds = epoll_wait(epfd, events, EPOLL_MAX_EVENTS, BUSY_POLL_US);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == sock_from_sender) {
                // Drain all DATA packets from sender
                while (true) {
                    ssize_t n = recvfrom(sock_from_sender, &pkt, sizeof(pkt),
                                        0, nullptr, nullptr);
                    if (n > 0) {
                        // Forward to receiver (send directly, no memcpy of payload)
                        sendto(sock_from_sender, &pkt, sizeof(pkt),
                               0, (sockaddr*)&dest_receiver, sizeof(dest_receiver));
                    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;  // no more data
                    } else {
                        // Error – ignore for robustness
                        break;
                    }
                }
            } else if (fd == sock_from_receiver) {
                // Drain all ACKs from receiver
                while (true) {
                    ssize_t n = recvfrom(sock_from_receiver, &pkt, sizeof(pkt),
                                        0, nullptr, nullptr);
                    if (n > 0) {
                        // Forward ACK back to sender
                        sendto(sock_from_receiver, &pkt, sizeof(pkt),
                               0, (sockaddr*)&dest_sender, sizeof(dest_sender));
                    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    } else {
                        break;
                    }
                }
            }
        }
    }

    close(sock_from_sender);
    close(sock_from_receiver);
    close(epfd);
    return 0;
}