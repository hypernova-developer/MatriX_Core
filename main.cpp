#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <algorithm>

std::string GLOBAL_TOKEN;

// Optimized Stockfish Call
std::string get_best_move(std::string moves) {
    std::string command = "stockfish <<EOF\nuci\nposition startpos moves " + moves + "\ngo movetime 1500\nquit\nEOF";
    std::array<char, 512> buffer;
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

void make_move(std::string game_id, std::string moves) {
    std::string best = get_best_move(moves);
    if (best != "error" && best.length() >= 4) {
        std::cout << "[ACTION] Sending move " << best << " for game " << game_id << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) {
        std::cerr << "[ERROR] LICHESS_TOKEN not found!" << std::endl;
        return 1;
    }
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.4: The Finisher - Online" << std::endl;

    // We use stdbuf to disable output buffering for real-time logs
    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    
    char buffer[1024]; 
    std::string line_accumulator;

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        line_accumulator += buffer;

        // If the line is not complete (Lichess sends chunks), wait for the next chunk
        if (line_accumulator.find('\n') == std::string::npos) continue;

        std::string event = line_accumulator;
        line_accumulator.clear();

        if (event.length() < 5) continue; // Keep-alive lines

        // 1. CHALLENGE DETECTION
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, event.find("\"", id_s) - id_s);
            std::cout << "[EVENT] Challenge received: " << c_id << std::endl;
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
        } 
        
        // 2. GAME STATE DETECTION
        else if (event.find("\"type\":\"gameStart\"") != std::string::npos || event.find("\"moves\"") != std::string::npos) {
            size_t g_s = event.find("\"id\":\"");
            if (g_s == std::string::npos) g_s = event.find("\"gameId\":\"");
            
            if (g_s != std::string::npos) {
                g_s = event.find(":", g_s) + 2;
                std::string g_id = event.substr(g_s, event.find("\"", g_s) - g_s);
                
                size_t m_s = event.find("\"moves\":\"");
                std::string moves = "";
                if (m_s != std::string::npos) {
                    m_s += 9;
                    moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                }
                
                // Simple turn logic: If moves are even, White plays. If odd, Black.
                // You'll need to know if your bot is White or Black, but for now let's just try to move.
                make_move(g_id, moves);
            }
        }
    }
    pclose(pipe);
    return 0;
}
