#pragma once
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(...) do { if (!(__VA_ARGS__)) throw std::runtime_error( \
    std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #__VA_ARGS__); } while (false)
template<class Exception, class Function> void check_throws(Function f) {
    bool caught=false;
    try { f(); } catch (const Exception&) { caught=true; }
    CHECK(caught);
}
template<class Function> int run_tests(Function f) {
    try { f(); std::cout << "PASS\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

