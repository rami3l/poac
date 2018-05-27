#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>

#include "include/poac.hpp"


void error_handling(std::string&& s) {
    poac::console::color::red();
    std::cerr << "Error: " << s << std::endl << std::endl;
    poac::console::color::reset();
}

template <typename VS>
int exec(VS&& vs) {
    try { // argv[0]: poac, argv[1]: new, ...
        poac::inference::apply("exec", vs);
        return EXIT_SUCCESS;
    }
    catch (const std::invalid_argument& e) {
        error_handling(std::move(e.what()));
        exec("--help");
        return EXIT_FAILURE;
    }
    catch (...) {
        error_handling("unexpected error");
        exec("--help");
        return EXIT_FAILURE;
    }
}

int main(int argc, const char** argv) {
    if (argc > 1) { return exec(argv[1]); }
    else { exec("--help"); return EXIT_FAILURE; }
} // std::vector<std::string>(argv+1, argv+argc)
