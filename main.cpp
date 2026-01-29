#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // UCI protocol call
    std::string command = "stockfish <<EOF\nuci\nposition startpos moves " + moves + "\ngo movetime 1000\nquit\nEOF";
    std::array<char, 512> buffer;
    std::string result = "";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "error";
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line = buffer.data();
        size_t bestmove_pos = line.find("bestmove ");
        if (bestmove_pos != std::string::npos) {
            result = line.substr(bestmove_pos + 9, 4); // "e2e4" format
            break;
        }
    }
    pclose(pipe);
    return result.empty() ? "error" : result;
}

void make_move(std::string game_id, std::string moves) {
    std::string best = get_best_move(moves);
    // Safety check: Don't send empty or error moves
    if (best != "error" && best.length() >= 4) {
        std::cout << "[ACTION] Sending move " << best << " to Game ID: " << game_id << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.5: ID Fixer Online" << std::endl;

    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        
        // 1. CHALLENGE DETECTION
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, event.find("\"", id_s) - id_s);
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
            std::cout << "[EVENT] Accepted Challenge: " << c_id << std::endl;
        } 
        // 2. GAME STATE DETECTION (The Critical Part)
        else if (event.find("\"game\":{") != std::string::npos || event.find("\"type\":\"gameStart\"") != std::string::npos) {
            // Extract the 8-character Game ID correctly
            size_t id_pos = event.find("\"id\":\"") + 6;
            if (id_pos < 6) id_pos = event.find("\"gameId\":\"") + 10;
            
            std::string g_id = event.substr(id_pos, 8); // Game IDs are exactly 8 chars
            
            size_t m_s = event.find("\"moves\":\"");
            std::string moves = "";
            if (m_s != std::string::npos) {
                m_s += 9;
                moves = event.substr(m_s, event.find("\"", m_s) - m_s);
            }
            
            make_move(g_id, moves);
        }
    }
    pclose(pipe);
    return 0;
}
