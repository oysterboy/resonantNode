#pragma once

#include "SerialOut.h"

/*
printKv (step 12.4.6)

One " key=value" token of a console line: printKv("phase", x) prints the same bytes as print(" phase="); print(x),
through the same Print overload, so a reporter reads as the list of keys it prints. The line's head and its end
(println) stay explicit at the call site.
*/
template <typename T>
inline void printKv(const char* key, const T& value) {
    serialOut().print(' ');
    serialOut().print(key);
    serialOut().print('=');
    serialOut().print(value);
}

// A float with `digits` decimals (Print's default is 2).
template <typename T>
inline void printKv(const char* key, const T& value, int digits) {
    serialOut().print(' ');
    serialOut().print(key);
    serialOut().print('=');
    serialOut().print(value, digits);
}
