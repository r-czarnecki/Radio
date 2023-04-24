#ifndef ARGHOLDER_H
#define ARGHOLDER_H

#include <map>
#include <string>

class ArgHolder {
public:
    ArgHolder();
    std::string getArg(std::string name);
    const char* getArgCstr(std::string name);
    void initArgument(std::string arg, bool is_optional, std::string default_value = "");
    void setValues(int argc, char *argv[]);
private:
    struct Argument {
        std::string value;
        bool is_optional;
    };

    void setValue(std::string arg, std::string value);
    bool checkArguments();

    std::map<std::string, Argument> arguments;
};

#endif