#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

std::string ask_stockfish(std::string fen) {
    std::string command = "stockfish <<EOF\nposition fen " + fen + "\ngo movetime 1000\nquit\nEOF";
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    
    if (!pipe) throw std::runtime_error("Engine failure.");
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        if (line.find("bestmove") != std::string::npos) {
            return line.substr(9, 4);
        }
    }
    return "error";
}

int main() {
    const char* token = std::getenv("LICHESS_TOKEN");
    if (!token) return 1;

    std::cout << "Matrix-Core Engine Status: Online" << std::endl;
    
    std::string start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::string move = ask_stockfish(start_pos);
    
    std::cout << "Engine Analysis: " << move << std::endl;
    
    return 0;
}
