#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <iostream>
#include <arpa/inet.h>
#include <regex>

#include "../include/communicator.h"
#include "../include/err.h"

static void *holdConnection(void *data) {
    Communicator *com = (Communicator *)data;

    while(!com->isStopped()) {
        if (com->hasCurrent()) {
            sockaddr_in current = com->getCurrentRadio();
            com->sendMessage(3, "", &current);
        }
        
        usleep(3500000);
    }

    return nullptr;
}

const int Communicator::BufferSize = 10000;

Communicator::Communicator(ArgHolder& arguments, TelnetMenu& menu)
: arguments(arguments)
, menu(menu)
, has_current(false)
, sock(0)
, address()
, current_radio()
, recv_from_current(false)
, stop(false) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&th, &attr, holdConnection, this);

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0)
        syserr("setsockopt");

    timeval t;
    t.tv_sec = std::stoi(arguments.getArg("T"));
    t.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0)
        syserr("setsockopt");

    addrinfo addr_hints;
    addrinfo *addr_result;
    memset(&addr_hints, 0, sizeof(addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;

    if (getaddrinfo(arguments.getArgCstr("H"), arguments.getArgCstr("P"), &addr_hints, &addr_result) != 0)
        syserr("getaddrinfo");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = ((sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr;
    address.sin_port = htons(std::stoi(arguments.getArg("P")));
}

Communicator::~Communicator() {
    close(sock);
}

void Communicator::run() {
    char buffer[BufferSize];
    sockaddr_in sender;
    socklen_t len = sizeof(sender);
    while (!stop) {
        int ret = recvfrom(sock, buffer, BufferSize, 0, (sockaddr *)&sender, &len);

        if (ret < 0 && !stop)
            menu.deselect();
        else if (ret > 0)
            parseMessage(std::string(buffer, ret), sender);
    }

    pthread_join(th, nullptr);
}

void Communicator::sendMessage(int type, std::string data, sockaddr_in *to) {
    int16_t header[2];
    header[0] = htons(type);
    header[1] = htons(data.size());
    
    if (to == nullptr)
        to = &address;
    sendto(sock, (char *)header, 4, 0, (sockaddr *)to, sizeof(*to));
    sendto(sock, data.c_str(), data.size(), 0, (sockaddr *)to, sizeof(*to));
}

void Communicator::stopWork() {
    stop = true;
}

void Communicator::parseMessage(std::string message, sockaddr_in sender) {
    static uint16_t type = 0;
    static uint16_t remaining_data = 0;
    static std::string data = "";
    static int bytes_read = 0;

    for (uint8_t c : message) {
        if (bytes_read < 2) {
            type <<= 8;
            type |= c;
            remaining_data = 0;
            bytes_read++;
        }
        else if (bytes_read < 4) {
            remaining_data <<= 8;
            remaining_data |= c;
            bytes_read++;

            if (bytes_read == 4 && remaining_data == 0)
                bytes_read = 0;
        }
        else {
            remaining_data--;
            switch (type) {
                case 2:
                    data += c;
                    if (remaining_data == 0)
                        menu.addRadio(data, sender);
                    break;
                case 4:
                    if (!has_current || sender.sin_addr.s_addr != current_radio.sin_addr.s_addr || sender.sin_port != current_radio.sin_port)
                        break;

                    printf("%c", c);
                    break;
                case 6:
                    if (!has_current || sender.sin_addr.s_addr != current_radio.sin_addr.s_addr || sender.sin_port != current_radio.sin_port)
                        break;

                    fprintf(stderr, "%c", c);
                    data += c;

                    if (remaining_data == 0) {
                        std::regex reg("StreamTitle='([^']*)'");
                        std::smatch match;
                        if (std::regex_search(data, match, reg))
                            menu.setMeta(match[1].str());
                    }
            }

            if (remaining_data <= 0) {
                data.clear();
                bytes_read = 0;
            }
            else
                bytes_read++;
        }
    }
}

void Communicator::removeCurrent() {
    has_current = false;
}

void Communicator::setCurrent(sockaddr_in current) {
    current_radio = current;
    has_current = true;
}

bool Communicator::hasCurrent() {
    return has_current;
}

bool Communicator::isStopped() {
    return stop;
}

sockaddr_in Communicator::getCurrentRadio() {
    return current_radio;
}