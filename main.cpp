#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // UCI protocol and increased thinking time for stability
    std::string command = "stockfish <<EOF\nuci\nposition startpos moves " + moves + "\ngo movetime 1500\nquit\nEOF";
    std::array<char, 256> buffer;
    std::string result = "error";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "error";
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line = buffer.data();
        if (line.find("bestmove") != std::string::npos) {
            size_t pos = line.find("bestmove") + 9;
            result = line.substr(pos, 4);
            break;
        }
    }
    pclose(pipe);
    return result;
}

void send_move(std::string game_id, std::string moves) {
    std::string best = get_best_move(moves);
    if (best != "error" && best.length() >= 4) {
        std::cout << ">>> Matrix-Core is playing: " << best << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.3: Iron Guard Active" << std::endl;

    // We are adding -sS to curl for better error reporting
    std::string stream_cmd = "curl -sS -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    if (!pipe) return 1;

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        if (event.length() < 10) continue; // Skip empty keep-alive lines

        std::cout << "DEBUG: " << event << std::endl; // THIS WILL SHOW US WHAT LICHESS SAYS

        // CHALLENGE DETECTION
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, event.find("\"", id_s) - id_s);
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
        } 
        // GAME & MOVE DETECTION
        else if (event.find("\"type\":\"gameStart\"") != std::string::npos || event.find("\"type\":\"gameState\"") != std::string::npos || event.find("\"moves\"") != std::string::npos) {
            
            // Extract Game ID
            size_t g_s = event.find("\"id\":\"");
            if(g_s == std::string::npos) g_s = event.find("\"gameId\":\""); // Some events use gameId
            if(g_s != std::string::npos) {
                g_s += (event.at(g_s+1) == 'g' ? 10 : 6);
                std::string g_id = event.substr(g_s, event.find("\"", g_s) - g_s);
                
                // Extract Moves
                size_t m_s = event.find("\"moves\":\"");
                if (m_s != std::string::npos) {
                    m_s += 9;
                    std::string moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                    send_move(g_id, moves);
                } else {
                    send_move(g_id, ""); // First move for White
                }
            }
        }
    }
    pclose(pipe);
    return 0;
}
