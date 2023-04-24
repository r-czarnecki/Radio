#include <iostream>
#include <pthread.h>

#include "../include/argHolder.h"
#include "../include/radio.h"
#include "../include/receiver.h"
#include "../include/err.h"

void *receiverThread(void* data) {
    Receiver *receiver = (Receiver *)data;
    receiver->run();
    return nullptr;
}

void initArguments(ArgHolder& arguments) {
    arguments.initArgument("h", false);
    arguments.initArgument("r", false);
    arguments.initArgument("p", false);
    arguments.initArgument("m", true, "no");
    arguments.initArgument("t", true, "5");

    arguments.initArgument("P", true);
    arguments.initArgument("B", true);
    arguments.initArgument("T", true, "5");
}

int main(int argc, char *argv[]) {
    pthread_mutex_t sync;
    if (pthread_mutex_init(&sync, 0) != 0)
        fatal("mutex_init");

    ArgHolder arg_holder{};
    initArguments(arg_holder);
    arg_holder.setValues(argc, argv);

    Radio radio(arg_holder, sync);
    Receiver receiver(radio, arg_holder, sync);

    pthread_t th;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_create(&th, &attr, receiverThread, &receiver);

    radio.run();
    pthread_join(th, nullptr);
    pthread_mutex_destroy(&sync);
}