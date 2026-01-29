#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <vector>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // Explicit UCI handshake for stability
    std::string command = "echo \"uci\nposition startpos moves " + moves + "\ngo movetime 1000\nquit\" | stockfish";
    std::array<char, 512> buffer;
    std::string result = "";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "error";
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line = buffer.data();
        size_t bestmove_pos = line.find("bestmove ");
        if (bestmove_pos != std::string::npos) {
            result = line.substr(bestmove_pos + 9, 4);
            break;
        }
    }
    pclose(pipe);
    return result;
}

void make_move(std::string game_id, std::string moves) {
    // BLOCK the "muhammed" fake ID
    if (game_id.find("muham") != std::string::npos) return;

    std::string best = get_best_move(moves);
    if (best != "error" && best.length() >= 4 && best != " (no") {
        std::cout << "[ACTION] Valid Game ID: " << game_id << " | Move: " << best << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.7: The Purifier Online" << std::endl;

    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    
    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        if (event.length() < 10) continue;

        // 1. Challenge Acceptance
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, 8);
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
            std::cout << "[EVENT] Accepted Challenge ID: " << c_id << std::endl;
            continue;
        }

        // 2. Focused Game ID Extraction
        // Look for gameId field which is much more reliable than generic id
        size_t g_id_pos = event.find("\"gameId\":\"");
        if (g_id_pos == std::string::npos) g_id_pos = event.find("\"id\":\"");

        if (g_id_pos != std::string::npos) {
            size_t start = (event.find("gameId") != std::string::npos) ? g_id_pos + 10 : g_id_pos + 6;
            std::string potential_id = event.substr(start, 8);

            // Double check: Game IDs never contain "muham"
            if (potential_id.find("muham") == std::string::npos) {
                size_t m_s = event.find("\"moves\":\"");
                std::string moves = "";
                if (m_s != std::string::npos) {
                    m_s += 9;
                    moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                }
                make_move(potential_id, moves);
            }
        }
    }
    pclose(pipe);
    return 0;
}
