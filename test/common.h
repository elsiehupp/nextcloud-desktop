#include <thread>

#ifndef COMMON_H
#define COMMON_H

template <typename T>
void wait(T duration)
{
    std::this_thread::sleep_for(duration);
}

#endif // COMMON_H
