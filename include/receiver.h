#ifndef RECEIVER_H
#define RECEIVER_H

#include <poll.h>
#include <map>
#include <sys/time.h>
#include <queue>

#include "radio.h"

struct Message {
    int16_t type;
    int16_t data_length;
    int bytes_read;
    bool ready;
};

struct ClientInfo {
    void init(sockaddr_in address, int sock);
    sockaddr_in address;
    int sock;
    timeval last_message;
    bool send_data;
    Message message;
};

class Receiver {
public:
    Receiver(Radio& radio, ArgHolder& arguments, pthread_mutex_t& sync);
    ~Receiver();
    void run();
    void stopWork();
    void sendMessage(int type, std::string data, ClientInfo& info);
    void removeNotResponding();
private:
    void readFrom(int sock);
    void respondTo(ClientInfo& info);

    Radio& radio;
    ArgHolder& arguments;
    pthread_mutex_t& sync;
    volatile bool stop;
    pollfd sockets[2];
    sockaddr_in address;
    std::map<std::string, ClientInfo> clients;
};

#endif