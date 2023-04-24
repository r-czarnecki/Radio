#include "../include/argHolder.h"
#include "../include/err.h"

ArgHolder::ArgHolder()
: arguments() {}

std::string ArgHolder::getArg(std::string name) {
    return arguments[name].value;
}

const char* ArgHolder::getArgCstr(std::string name) {
    return arguments[name].value.c_str();
}

void ArgHolder::initArgument(std::string arg, bool is_optional, std::string default_value) {
    arguments[arg] = {default_value, is_optional};
}

void ArgHolder::setValues(int argc, char *argv[]) {
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc)
            fatal("Not enough arguments.");

        if (argv[i][0] != '-')
            fatal("Wrong argument %s", argv[i]);

        std::string arg = std::string(argv[i]).substr(1);
        setValue(arg, argv[i + 1]);
    }

    if(!checkArguments())
        fatal("Wrong arguments.");
}

void ArgHolder::setValue(std::string arg, std::string value) {
    if (arguments.count(arg) == 0)
        fatal("Wrong argument -%s", arg.c_str());

    arguments[arg].value = value;
}

bool ArgHolder::checkArguments() {
    for (auto& element : arguments) {
        if (!element.second.is_optional && element.second.value == "")
            return false;
    }

    return true;
}