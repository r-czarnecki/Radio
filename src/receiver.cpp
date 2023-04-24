#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "../include/receiver.h"
#include "../include/err.h"

Receiver::Receiver(Radio& radio, ArgHolder& arguments, pthread_mutex_t& sync)
: radio(radio)
, arguments(arguments)
, sync(sync)
, stop(false)
, sockets()
, address()
, clients() {
    radio.setClientMap(&clients);
    radio.setReceiver(this);

    std::string port = arguments.getArg("P");
    if (port == "")
        return;

    pollfd sock;
    sock.fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sock.events = POLLIN;
    sock.revents = 0;

    if (sock.fd < 0)
        syserr("socket");

    int opt = 1;
    if (setsockopt(sock.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        syserr("setsockopt");

    opt = 1;
    if (setsockopt(sock.fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0)
        syserr("setsockopt");

    int flags = fcntl(sock.fd, F_GETFL);
    if (flags < 0)
        syserr("get flags");

    if (fcntl(sock.fd, F_SETFL, flags | O_NONBLOCK) < 0)
        syserr("set flag");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(std::stoi(port));

    if (bind(sock.fd, (sockaddr *)&address, sizeof(address)) < 0)
        syserr("bind");
    sockets[0] = sock;

    sock = {0, POLLIN, 0};
    std::string multiaddr = arguments.getArg("B");
    if (multiaddr != "") {
        sock.fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock.fd < 0)
            syserr("socket");

        opt = 1;
        if (setsockopt(sock.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            syserr("setsockopt");

        ip_mreq ip_mreq;
        ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (inet_aton(multiaddr.c_str(), &ip_mreq.imr_multiaddr) == 0)
            fatal("invalid multicast address");

        if (setsockopt(sock.fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &ip_mreq, sizeof(ip_mreq)) < 0)
            syserr("setsockopt");

        if (bind(sock.fd, (sockaddr *)&address, sizeof(address)) < 0)
            syserr("bind");
    }
    sockets[1] = sock;
}

Receiver::~Receiver() {
    for (int i = 0; i < 2; i++)
        close(sockets[i].fd);
}

void Receiver::run() {
    while(!stop) {
        if (poll(sockets, 2, 1000) == 0)
            continue;

        for (int i = 0; i < 2; i++) {
            if (sockets[i].revents & POLLIN) {
                readFrom(sockets[i].fd);
                sockets[i].revents = 0;
            }
        }
    }
}

void Receiver::stopWork() {
    stop = true;
}

void Receiver::sendMessage(int type, std::string data, ClientInfo& info) {
    if (!info.send_data && type != 2)
        return;

    uint16_t header[2];
    header[0] = htons(type);
    header[1] = htons(data.size());
        
    data.insert(0, (char *)header, 4);

    if (sendto(info.sock, data.c_str(), data.size(), 0, (sockaddr *)&info.address, sizeof(info.address)) < 0)
        syserr("sendto");
}

void Receiver::removeNotResponding() {
    timeval current_time, difference;
    gettimeofday(&current_time, 0);

    for (auto it = clients.begin(); it != clients.end(); it++) {
        ClientInfo& client = it->second;
        timersub(&current_time, &client.last_message, &difference);
        if (difference.tv_sec >= std::stoi(arguments.getArg("T"))) {
            auto tmp = it;
            it--;
            clients.erase(tmp);
        }
    }
}

void ClientInfo::init(sockaddr_in address, int sock) {
    this->address = address;
    this->sock = sock;
    send_data = false;
    gettimeofday(&last_message, 0);
    message = {0, 0, 0, false};
}

void Receiver::readFrom(int sock) {
    char buffer[4];
    int len;
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    do {
        len = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr *)&client_addr, &client_len);
        if (len >= 0) {
            std::string id = inet_ntoa(client_addr.sin_addr) + std::string(";") + std::to_string(ntohs(client_addr.sin_port));

            pthread_mutex_lock(&sync);
            if (clients.count(id) == 0) {
                ClientInfo info;
                info.init(client_addr, sock);
                clients[id] = info;
            }

            ClientInfo info = clients[id];
            Message& message = info.message;
            for (int i = 0; i < len; i++) {
                if (message.bytes_read < 4) {
                    if(message.bytes_read >= 2) {
                        message.data_length <<= 8;
                        message.data_length |= buffer[i];
                    }
                    else {
                        message.type <<= 8;
                        message.type |= buffer[i];
                    }
                }

                message.bytes_read++;
                if (message.bytes_read == message.data_length + 4) {
                    message.ready = true;
                    respondTo(info);
                }
            }
            gettimeofday(&info.last_message, 0);
            clients[id] = info;
            pthread_mutex_unlock(&sync);
        }
    } while(len > 0);
}

void Receiver::respondTo(ClientInfo& info) {
    std::string id = inet_ntoa(info.address.sin_addr) + std::string(";") + std::to_string(ntohs(info.address.sin_port));
    if (info.message.type == 1) {
        std::string name = radio.getRadioName();
        sendMessage(2, name, info);
        info.send_data = true;
        sendMessage(6, radio.getMeta(), info);
    }
    else if (info.message.type == 3) {
        sendMessage(6, radio.getMeta(), info);
        info.send_data = true;
    }

    info.message = {0, 0, 0, false};
}