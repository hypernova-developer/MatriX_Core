#include <iostream>
#include <cstdlib>
#include <string>

int main() {
    const char* token = std::getenv("LICHESS_TOKEN");

    if (token == nullptr) {
        std::cerr << "CRITICAL ERROR: LICHESS_TOKEN not found in environment!" << std::endl;
        return 1;
    }

    std::cout << "--- Matrix-Core v1.0 ---" << std::endl;
    std::cout << "License: GNU GPL v3.0" << std::endl;
    std::cout << "Status: Online and ready for challenges." << std::endl;

    std::string command = "curl -s -H \"Authorization: Bearer " + std::string(token) + "\" https://lichess.org/api/account";
    
    int result = std::system(command.c_str());

    if (result == 0) {
        std::cout << "\nConnection Successful!" << std::endl;
    } else {
        std::cout << "\nConnection Failed!" << std::endl;
    }

    return 0;
}
