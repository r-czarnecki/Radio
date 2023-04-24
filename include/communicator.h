#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include "argHolder.h"
#include "telnetMenu.h"

class Communicator {
public:
    Communicator(ArgHolder& arguments, TelnetMenu& menu);
    ~Communicator();
    void run();
    void setCurrent(sockaddr_in current);
    void removeCurrent();
    void sendMessage(int type, std::string data, sockaddr_in *to = nullptr);
    void stopWork();
    bool hasCurrent();
    bool isStopped();
    sockaddr_in getCurrentRadio();

private:
    void parseMessage(std::string message, sockaddr_in sender);

    static const int BufferSize;

    ArgHolder& arguments;
    TelnetMenu& menu;
    volatile bool has_current;
    int sock;
    sockaddr_in address;
    sockaddr_in current_radio;
    bool recv_from_current;
    volatile bool stop;
    pthread_t th;
};

#endif