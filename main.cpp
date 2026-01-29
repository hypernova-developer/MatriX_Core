#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>

// Global token for easier access in recursive calls
std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // Engine call with UCI protocol
    std::string command = "stockfish <<EOF\nuci\nposition startpos moves " + moves + "\ngo movetime 1000\nquit\nEOF";
    std::array<char, 128> buffer;
    std::string result = "error";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "error";
    
    char line_buf[256];
    while (fgets(line_buf, sizeof(line_buf), pipe) != nullptr) {
        std::string line(line_buf);
        if (line.find("bestmove") != std::string::npos) {
            result = line.substr(9, 4); // Example: e2e4
            break;
        }
    }
    pclose(pipe);
    return result;
}

void make_move(std::string game_id, std::string moves) {
    std::string best = get_best_move(moves);
    if (best != "error" && best != " (no") {
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
        std::cout << "Sent move: " << best << std::endl;
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.2: Aggressive Striker Online" << std::endl;

    // Listen for ALL events
    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    char buffer[16384];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        
        // Handle Challenges
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, event.find("\"", id_s) - id_s);
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
        } 
        // Handle Game Moves / Starts
        else if (event.find("\"type\":\"gameStart\"") != std::string::npos || event.find("\"moves\":\"") != std::string::npos) {
            size_t g_s = event.find("\"id\":\"") + 6;
            std::string g_id = event.substr(g_s, event.find("\"", g_s) - g_s);
            
            size_t m_s = event.find("\"moves\":\"");
            if (m_s != std::string::npos) {
                m_s += 9;
                std::string moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                make_move(g_id, moves);
            } else {
                // Game just started and no moves yet (Bot is White)
                make_move(g_id, "");
            }
        }
    }
    pclose(pipe);
    return 0;
}
