#ifndef COMPAREVERSION_H_
#define COMPAREVERSION_H_

#include <cstdio>
#include <string>
#include <iostream>

using namespace std;
struct CompareVersion {
public:
    int maj;
    int min;
    int rev;
    int build;
    CompareVersion(std::string);
    bool operator < (const CompareVersion&);
    bool operator <= (const CompareVersion&);
    bool operator == (const CompareVersion&);
    friend std::ostream& operator << (std::ostream& stream, const CompareVersion& ver) {
        stream << ver.maj;
        stream << '.';
        stream << ver.min;
        stream << '.';
        stream << ver.rev;
        stream << '.';
        stream << ver.build;
        return stream;
    };
    void reset();
};
#endif /* COMPAREVERSION_H_ */
