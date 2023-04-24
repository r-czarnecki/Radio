#ifndef TELNETMENU_H
#define TELNETMENU_H

#include <vector>
#include <functional>
#include <netinet/in.h>
#include <pthread.h>

#include "argHolder.h"

#define BUFFERSIZE 2000

class Communicator;

class TelnetMenu {
public:
    TelnetMenu(ArgHolder& arguments);
    ~TelnetMenu();
    void run(pthread_t& th);
    void deselect();
    void addRadio(std::string name, sockaddr_in addr);
    void setMeta(std::string song_name);
    void setCommunicator(Communicator* com);
    void drawMenu();
    void stopWork();
private:
    void parseMessage(std::string message);
    void moveSelection(int move);
    void click();
    void select();
    void clearScreen();

    static const char SE;
    static const char SB;
    static const char WILL;
    static const char DO;
    static const char IAC;

    struct Option {
        std::string string;
        sockaddr_in addr;
        std::function<void()> action;
    };

    ArgHolder& arguments;
    bool stop;
    int tcp_sock;
    int msg_sock;
    std::vector<Option> options;
    int position;
    int selected;
    Communicator* communicator;
    std::string song_name;
    pthread_mutex_t mut;
};

#endif