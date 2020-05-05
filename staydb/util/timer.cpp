#include <staydb/util/timer.h>
#include <chrono>

decltype(std::chrono::high_resolution_clock::now()) start;

void timer_start(){
    start = std::chrono::high_resolution_clock::now();
}

std::string nanoseconds_since_start(){
    auto finish = std::chrono::high_resolution_clock::now();
    std::string duration = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count());
    return duration;
}