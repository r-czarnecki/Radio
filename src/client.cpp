#include <signal.h>

#include "../include/argHolder.h"
#include "../include/telnetMenu.h"
#include "../include/communicator.h"

void initArguments(ArgHolder& arguments) {
    arguments.initArgument("H", false);
    arguments.initArgument("P", false);
    arguments.initArgument("p", false);
    arguments.initArgument("T", true, "5");
}

void *communicator(void *data) {
    Communicator *comm = (Communicator *)data;
    comm->run();
    return nullptr;
}

int main(int argc, char *argv[]) {
    ArgHolder arguments;
    initArguments(arguments);
    arguments.setValues(argc, argv);

    while (true) {
        TelnetMenu menu(arguments);
        Communicator comm(arguments, menu);
        menu.setCommunicator(&comm);

        pthread_attr_t attr;
        pthread_t th;
        pthread_attr_init(&attr);

        pthread_create(&th, &attr, communicator, &comm);

        menu.run(th);
        pthread_join(th, nullptr);
    }
}