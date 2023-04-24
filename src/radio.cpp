#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "../include/radio.h"
#include "../include/receiver.h"
#include "../include/err.h"

const int Radio::BufferSize = 5000;

Radio::Radio(ArgHolder& arguments, pthread_mutex_t& sync)
: arguments(arguments)
, sync(sync)
, buffer()
, stream_name()
, meta()
, metaint(0)
, radio_sock(0)
, addr_result(nullptr)
, clients(nullptr) {}

void Radio::run() {
    char cbuffer[BufferSize];
    makeConnection();
    bool data = false;
    
    buffer.clear();
    while (true) {
        int r = read(radio_sock, cbuffer, BufferSize);
        
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            syserr("read");
        }

        std::string tmp(cbuffer, r);
        if (!data)
            data = parseHeader(tmp);
        else
            writeData(tmp);
    }

    receiver->stopWork();
    close(radio_sock);
}

void Radio::makeConnection() {
    radio_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (radio_sock < 0)
        syserr("radio_sock");

    struct timeval t;
    t.tv_sec = std::stoi(arguments.getArg("t"));
    t.tv_usec = 0;
    if (setsockopt(radio_sock, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0)
        syserr("setsockopt SO_RCVTIMEO");

    struct addrinfo addr_hints;
    memset(&addr_hints, 0, sizeof(addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(arguments.getArgCstr("h"), arguments.getArgCstr("p"), &addr_hints, &addr_result) != 0)
        syserr("getaddrinfo");

    if (connect(radio_sock, addr_result->ai_addr, addr_result->ai_addrlen) != 0)
        syserr("connect to radio");
    freeaddrinfo(addr_result);

    buffer.clear();
    buffer = "GET " + arguments.getArg("r") + " HTTP/1.1" + CRLF;
    buffer += "Host: " + arguments.getArg("h") + ":" + arguments.getArg("p") + CRLF;
    buffer += std::string("Connection: close") + CRLF;

    if (arguments.getArg("m") == "yes")
        buffer += "Icy-Metadata: 1\r\n";

    buffer += CRLF;

    if (write(radio_sock, buffer.c_str(), buffer.size()) == -1)
        syserr("GET");
}

static inline bool isWhitespace(char c) {
    return (c == ' ' || c == '\t');
}

bool Radio::parseHeader(std::string data) {
    if (buffer.size() > BufferSize)
        buffer = buffer.substr(BufferSize);

    static int currently_reading = 0;
    static bool read_whitespace = false;
    static std::string response = "";
    static bool first_line = true;
    unsigned int i = buffer.size();
    buffer += data;

    for (; i < buffer.size(); i++) {
        if (first_line)
            response += buffer[i];

        if (buffer[i] == '\r')
            first_line = false;

        switch (currently_reading) {
            case 0:
                if (compareWithBuffer("\r\n\r\n", i)) {
                    if (strncmp(response.c_str(), "ICY 200 OK", 10) != 0 &&
                        strncmp(response.c_str(), "HTTP/1.0 200 OK", 15) != 0 &&
                        strncmp(response.c_str(), "HTTP/1.1 200 OK", 15) != 0)
                        fatal("Wrong response");

                    if (i + 1 != buffer.size())
                        writeData(buffer.substr(i + 1));
                    return true;
                }
                else if (compareWithBuffer("icy-name:", i)) {
                    read_whitespace = true;
                    currently_reading = 1;
                }
                else if (compareWithBuffer("icy-metaint:", i)) {
                    read_whitespace = true;
                    currently_reading = 2;
                }
                break;
            case 1:
                if (compareWithBuffer("\r\n", i)) {
                    currently_reading = 0;
                    stream_name.pop_back();
                    break;
                }

                if (read_whitespace && isWhitespace(buffer[i]))
                    break;

                read_whitespace = false;
                stream_name += buffer[i];
                break;
            case 2:
                if (compareWithBuffer("\r\n", i)) {
                    currently_reading = 0;
                    break;
                }

                if (read_whitespace && isWhitespace(buffer[i]))
                    break;

                if (buffer[i] - '0' < 0 || buffer[i] - '0' > 9)
                    break;

                read_whitespace = false;
                metaint *= 10;
                metaint += buffer[i] - '0';
                break;
        }
    }

    return false;
}

void Radio::writeData(std::string data) {
    static int remaining_data = metaint;
    static bool read_metalen = false;
    static int remaining_meta = 0;

    if (arguments.getArg("m") == "no") {
        if (arguments.getArg("P") != "")
            respondToClients(4, data);
        else
            for (char c : data)
                printf("%c", c);
    }
    else {
        for (char c : data) {
            if (read_metalen) {
                remaining_meta = (0 | c);
                remaining_meta *= 16;
                remaining_data = metaint;
                read_metalen = false;
                buffer.clear();
            }
            else if (remaining_meta != 0) {
                remaining_meta--;
                if (arguments.getArg("P") != "") {
                    buffer += c;
                    if (remaining_meta == 0 || buffer.size() == BufferSize) {
                        meta = buffer;
                        respondToClients(6, buffer);
                        buffer.clear();
                    }
                }
                else
                    fprintf(stderr, "%c", c);
            }
            else {
                remaining_data--;
                
                if (arguments.getArg("P") != "") {
                    buffer += c;
                    if (remaining_data == 0 || buffer.size() == BufferSize) {
                        respondToClients(4, buffer);
                        buffer.clear();
                    }
                }
                else
                    printf("%c", c);

                if (remaining_data == 0)
                    read_metalen = true;
            }
        }
    }
}

bool Radio::compareWithBuffer(std::string str, int buf_end_pos) {
    if (buf_end_pos - (int)str.size() + 1 < 0)
        return false;

    return buffer.compare(buf_end_pos - (int)str.size() + 1, str.size(), str) == 0;
}

void Radio::setClientMap(std::map<std::string, ClientInfo>* clients) {
    this->clients = clients;
}

void Radio::setReceiver(Receiver *receiver) {
    this->receiver = receiver;
}

void Radio::respondToClients(int type, std::string data) {
    if (clients == nullptr || receiver == nullptr)
        return;

    pthread_mutex_lock(&sync);
    receiver->removeNotResponding();
    for (auto& client : *clients)
        receiver->sendMessage(type, data, client.second);
    pthread_mutex_unlock(&sync);
}

std::string Radio::getRadioName() {
    return stream_name;
}

std::string Radio::getMeta() {
    return meta;
}