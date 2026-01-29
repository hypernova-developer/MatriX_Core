#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // We use a simpler, more direct UCI call
    std::string command = "echo \"uci\nposition startpos moves " + moves + "\ngo movetime 1000\" | stockfish";
    std::array<char, 512> buffer;
    std::string result = "";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "error";
    
    char line_buf[512];
    while (fgets(line_buf, sizeof(line_buf), pipe) != nullptr) {
        std::string line(line_buf);
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
    std::string best = get_best_move(moves);
    if (best != "error" && best.length() >= 4 && best != " (no") {
        std::cout << "[ACTION] Target ID: " << game_id << " | Move: " << best << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.6: Sniper Mode Online" << std::endl;

    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        if (event.length() < 10) continue;

        // CHALLENGE
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, 8); // IDs are 8 chars
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
            continue;
        }

        // IMPROVED GAME ID DETECTION
        // We look for "id":"..." specifically where it's 8 characters long
        size_t id_pos = 0;
        while ((id_pos = event.find("\"id\":\"", id_pos)) != std::string::npos) {
            std::string potential_id = event.substr(id_pos + 6, 8);
            // If the ID contains quotes or isn't 8 alphanumeric chars, it's probably a username
            if (potential_id.find("\"") == std::string::npos && potential_id.length() == 8) {
                
                size_t m_s = event.find("\"moves\":\"");
                std::string moves = "";
                if (m_s != std::string::npos) {
                    m_s += 9;
                    moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                }
                make_move(potential_id, moves);
                break; 
            }
            id_pos += 8;
        }
    }
    pclose(pipe);
    return 0;
}
