#ifndef RADIO_H
#define RADIO_H

#include <netdb.h>
#include <map>
#include <string>

#include "argHolder.h"

#define CRLF "\r\n"

struct ClientInfo;
class Receiver;

class Radio {
public:
    Radio(ArgHolder& arguments, pthread_mutex_t& sync);
    void run();
    void setClientMap(std::map<std::string, ClientInfo>* clients);
    void setReceiver(Receiver* clients);
    std::string getRadioName();
    std::string getMeta();
private:
    void makeConnection();
    bool parseHeader(std::string data);
    void writeData(std::string data);
    bool compareWithBuffer(std::string str, int buf_end_pos);
    void respondToClients(int type, std::string data);

    static const int BufferSize;

    ArgHolder& arguments;
    pthread_mutex_t& sync;
    std::string buffer;
    std::string stream_name;
    std::string meta;
    int metaint;
    int radio_sock;
    struct addrinfo *addr_result;
    std::map<std::string, ClientInfo>* clients;
    Receiver* receiver;
};

#endif