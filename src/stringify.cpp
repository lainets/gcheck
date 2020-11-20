#include <string>
#include <stack>
#include <cstring>
#include <sstream>

#include "stringify.h"
#include "user_object.h"

namespace gcheck {

std::string Escapees() {
    std::string escapees = "\\\"";
    for(char c = 0; c < 0x20; c++) {
        escapees.push_back(c);
    }
    return escapees;
}

std::string Replacee(char val) {
    static const char* digits = "0123456789ABCDEF";

    std::string ret = "\\u0000";
    for (size_t i = 0; i < 2; ++i)
        ret[i+4] = digits[(val >> 4*(1-i)) & 0xf];
    return ret;
}
std::vector<std::string> Replacees() {
    std::vector<std::string> replacees{"\\\\", "\\\""};
    for(char c = 0; c < 0x20; c++) {
        replacees.push_back(Replacee(c));
    }
    return replacees;
}

std::string UTF8Escape(std::string str) {
    static const std::string escapees = Escapees();
    static const std::vector<std::string> replacees = Replacees();

    size_t pos = 0;
    int index = 0;
    size_t min = std::string::npos;
    for(unsigned int i = 0; i < escapees.length(); i++) {
        size_t p = str.find(escapees[i], pos);
        if(p < min) {
            min = p;
            index = i;
        }
    }
    pos = min;
    while(pos != std::string::npos) {
        str.replace(pos, 1, replacees[index]);
        pos += replacees[index].length();

        index = 0;
        min = std::string::npos;
        for(unsigned int i = 0; i < escapees.length(); i++) {
            size_t p = str.find(escapees[i], pos);
            if(p < min) {
                min = p;
                index = i;
            }
        }
        pos = min;
    }

    return str;
}

std::string UTF8ify(std::string str) {
    static const std::string escapees = Escapees();
    static const std::vector<std::string> replacees = Replacees();

    // encode non-utf-8 characters
    std::stack<size_t> positions;
    for(size_t pos = 0; pos < str.length(); pos++) {
        int num = 0;
        while((str[pos] << num) & 0b10000000) num++;
        if(num == 0)
            continue;
        else if(num != 1) {
            int count = 0;
            while(count < num-1 && (str[pos+count+1] & 0b11000000) == 0b10000000) count++;
            if(count == num-1) {
                pos += count;
                continue;
            }
        }
        positions.push(pos);
    }

    str.resize(str.length()+positions.size()*5); // add space for encoding

    size_t epos = str.length()-1;
    size_t offset = positions.size()*5;
    char* cstr = str.data();
    while(!positions.empty()) {
        size_t pos = positions.top();
        auto repl = Replacee(str[pos]);

        std::memmove(cstr+offset+pos+1, cstr+pos+1, epos-offset-pos);
        offset -= 5;
        std::copy(repl.data(), repl.data()+6, cstr+offset+pos);
        epos = offset+pos-1;

        positions.pop();
    }

    return str;
}

std::string UTF8Encode(std::string str) {
    return UTF8ify(UTF8Escape(str));
}


std::string toConstruct(const char* const& item) { return '"' + UTF8Encode(item) + '"'; }
std::string toConstruct(const char*& item) { return (const char*)item; }
std::string toConstruct(const std::string& item) {
    return "std::string(" + toConstruct((const char*)item.c_str()) + ")";
}
std::string toConstruct(const unsigned char& item) { return "static_cast<unsigned char>(" + std::to_string((int)item) + ")"; }
std::string toConstruct(const char& item) { return "static_cast<char>(" + std::to_string((int)item) + ")"; }
std::string toConstruct(const bool& b) { return b ? "true" : "false"; }
#ifdef GCHECK_CONSTRUCT_DATA
std::string toConstruct(const UserObject& u) { return u.construct(); }
#else
std::string toConstruct(const UserObject&) { return ""; }
#endif
std::string toConstruct(decltype(nullptr)) { return "nullptr"; }
std::string toConstruct(const int& item) { return std::to_string(item); }
std::string toConstruct(const long& item) { return std::to_string(item) + 'L'; }
std::string toConstruct(const long long& item) { return std::to_string(item) + "LL"; }
std::string toConstruct(const unsigned& item) { return std::to_string(item) + 'U'; }
std::string toConstruct(const unsigned long& item) { return std::to_string(item) + "UL"; }
std::string toConstruct(const unsigned long long& item) { return std::to_string(item) + "ULL"; }

std::string toConstruct(const float& item) {
    std::stringstream ss;
    ss << std::hexfloat << item << 'f';
    return ss.str();
}

std::string toConstruct(const double& item) {
    std::stringstream ss;
    ss << std::hexfloat << item;
    return ss.str();
}

std::string toConstruct(const long double& item) {
    std::stringstream ss;
    ss << std::hexfloat << item << 'l';
    return ss.str();
}


std::string toString(const std::string& item) { return '"' + item + '"'; }
std::string toString(const char* const&item) { return toString(std::string(item)); }
std::string toString(const char*& item) { return toString((const char*)item); }
std::string toString(const char& item) { return std::string("'") + item + "'"; }
std::string toString(const bool& b) { return b ? "true" : "false"; }
std::string toString(const UserObject& u) { return u.string(); }
std::string toString(decltype(nullptr)) { return "nullptr"; }
std::string toString(const int& item) { return std::to_string(item); }
std::string toString(const long& item) { return std::to_string(item); }
std::string toString(const long long& item) { return std::to_string(item); }
std::string toString(const unsigned& item) { return std::to_string(item); }
std::string toString(const unsigned long& item) { return std::to_string(item); }
std::string toString(const unsigned long long& item) { return std::to_string(item); }
std::string toString(const float& item) { return std::to_string(item); }
std::string toString(const double& item) { return std::to_string(item); }
std::string toString(const long double& item) { return std::to_string(item); }

} // gcheck