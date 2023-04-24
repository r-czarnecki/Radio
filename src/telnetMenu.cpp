#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <fcntl.h>

#include "../include/telnetMenu.h"
#include "../include/err.h"
#include "../include/communicator.h"

const char TelnetMenu::SE = (char)240;
const char TelnetMenu::SB = (char)250;
const char TelnetMenu::WILL = (char)251;
const char TelnetMenu::DO = (char)253;
const char TelnetMenu::IAC = (char)255;

TelnetMenu::TelnetMenu(ArgHolder& arguments)
: arguments(arguments)
, stop(false)
, tcp_sock(0)
, msg_sock(0)
, options()
, position(0)
, selected(-1)
, communicator(nullptr)
, song_name("") {
    pthread_mutex_init(&mut, 0);

    tcp_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0)
        syserr("socket");

    int opt = 1;
    if (setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        syserr("setsockopt");

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(std::stoi(arguments.getArg("p")));

    if (bind(tcp_sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        syserr("bind");
    
    if (listen(tcp_sock, 5) < 0)
        syserr("connect");

    Option discover;
    discover.string = "Szukaj posrednika";
    discover.action = [this](){
        deselect();
        options.erase(options.begin() + 1, options.end() - 1);
        if (communicator != nullptr) {
            communicator->removeCurrent();
            communicator->sendMessage(1, "");
        }
    };
    options.push_back(discover);

    Option end;
    end.string = "Koniec";
    end.action = [this](){
        this->stop = true;
        communicator->stopWork();
    };
    options.push_back(end);
}

TelnetMenu::~TelnetMenu() {
    close(tcp_sock);
    close(msg_sock);
    pthread_mutex_destroy(&mut);
}

void TelnetMenu::run(pthread_t& th) {
    sockaddr_in telnet_addr;
    socklen_t telnet_addr_len = sizeof(telnet_addr);
    
    if ((msg_sock = accept(tcp_sock, (sockaddr *)&telnet_addr, &telnet_addr_len)) < 0)
        syserr("accept");

    int opt = 1;
    if (setsockopt(msg_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        syserr("setsockopt");

    char initTelnet[] = {IAC, DO, 34, IAC, SB, 34, 1, 0, IAC, SE, IAC, WILL, 1};
    if (write(msg_sock, initTelnet, 13) != 13)
        syserr("write");

    drawMenu();

    int tmp;
    char cbuffer[BUFFERSIZE];
    stop = false;
    while (!stop) {
        tmp = read(msg_sock, cbuffer, BUFFERSIZE);
        if (tmp <= 0) {
            stop = true;
            communicator->stopWork();
        }
        else
            parseMessage(std::string(cbuffer, tmp));
    }

    pthread_join(th, nullptr);
}

void TelnetMenu::deselect() {
    communicator->removeCurrent();
    selected = -1;
    song_name = "";
    drawMenu();
}

void TelnetMenu::addRadio(std::string name, sockaddr_in addr) {
    Option radio;
    radio.string = name;
    radio.addr = addr;
    radio.action = [this, addr](){
        communicator->setCurrent(addr);
        select();
        sockaddr_in tmp = addr;
        communicator->sendMessage(3, "", &tmp);
    };

    pthread_mutex_lock(&mut);
    bool already_exists = false;
    for (unsigned int i = 1; i < options.size() - 1 && !already_exists; i++)
        if (options[i].addr.sin_addr.s_addr == radio.addr.sin_addr.s_addr && 
            options[i].addr.sin_port == radio.addr.sin_port)
            already_exists = true;

    if (!already_exists) {
        options.insert(options.end() - 1, radio);
        drawMenu();
    }
    pthread_mutex_unlock(&mut);
}

void TelnetMenu::setMeta(std::string song_name) {
    this->song_name = song_name;
    drawMenu();
}

void TelnetMenu::setCommunicator(Communicator* com) {
    this->communicator = com;
}

void TelnetMenu::drawMenu() {
    if (msg_sock == 0)
        return;

    clearScreen();


    for (unsigned int i = 0; i < options.size(); i++) {
        Option opt = options[i];

        if (write(msg_sock, opt.string.c_str(), opt.string.size()) < 0)
            syserr("write");

        if (selected == (int)i)
            if (write(msg_sock, " *", 2) < 0)
                syserr("write");

        if (position == (int)i)
            if (write(msg_sock, " <", 2) < 0)
                syserr("write");

        if (write(msg_sock, "\n\r", 2) < 0)
            syserr("write");
    }

    if (write(msg_sock, song_name.c_str(), song_name.size()) < 0)
        syserr("write");
}

void TelnetMenu::parseMessage(std::string message) {
    static int key[10];
    static int key_bytes = 0;

    pthread_mutex_lock(&mut);
    for (char c : message) {
        key[key_bytes] = c;
        if (key_bytes == 0) {
            if (c == 27 || c == 13)
                key_bytes++;
            else
                key_bytes = 0;
        }
        else if (key_bytes == 1) {
            if (c == 0 && key[key_bytes - 1] == 13) {
                click();
                key_bytes = 0;
            }
            else if (c == 91 && key[key_bytes - 1] == 27)
                key_bytes++;
            else
                key_bytes = 0;
        }
        else if (key_bytes == 2) {
            if (c == 65)
                moveSelection(-1);
            else if (c == 66)
                moveSelection(1);
            key_bytes = 0;
        }
    }
    pthread_mutex_unlock(&mut);
}

void TelnetMenu::moveSelection(int move) {
    int new_pos = position + move;

    if (new_pos >= 0 && new_pos < (int)options.size())
        position = new_pos;

    drawMenu();
}

void TelnetMenu::click() {
    options[position].action();
    drawMenu();
}

void TelnetMenu::select() {
    selected = position;
    drawMenu();
}

void TelnetMenu::clearScreen() {
    char msg[] = "\x1B[2J\x1B[0;0H";
    if (write(msg_sock, msg, 10) < 0)
        syserr("clear screen");
}

void TelnetMenu::stopWork() {
    stop = true;
}