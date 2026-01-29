#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <chrono>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // We added more thinking time and explicit UCI protocol
    std::string command = "echo \"uci\nposition startpos moves " + moves + "\ngo movetime 2000\nquit\" | stockfish";
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
    if (game_id.find("muham") != std::string::npos) return;

    std::string best = get_best_move(moves);
    if (best != "" && best != "error" && best.length() >= 4 && best.find("(") == std::string::npos) {
        std::cout << "[ACTION] Playing " << best << " for Game: " << game_id << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        std::system(move_cmd.c_str());
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.8: Final Boss Mode Online" << std::endl;

    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
    
    // Outer loop to keep the bot alive even if connection drops
    while (true) {
        FILE* pipe = popen(stream_cmd.c_str(), "r");
        if (!pipe) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        char buffer[8192];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string event(buffer);
            if (event.length() < 5) continue;

            if (event.find("\"type\":\"challenge\"") != std::string::npos) {
                size_t id_s = event.find("\"id\":\"") + 6;
                std::string c_id = event.substr(id_s, 8);
                std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
            } 
            else if (event.find("\"gameId\":\"") != std::string::npos || event.find("\"id\":\"") != std::string::npos) {
                size_t g_pos = event.find("\"gameId\":\"");
                if (g_pos == std::string::npos) g_pos = event.find("\"id\":\"");
                size_t start = (event.find("gameId") != std::string::npos) ? g_pos + 10 : g_pos + 6;
                std::string g_id = event.substr(start, 8);

                if (g_id.find("muham") == std::string::npos) {
                    size_t m_s = event.find("\"moves\":\"");
                    std::string moves = "";
                    if (m_s != std::string::npos) {
                        m_s += 9;
                        moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                    }
                    make_move(g_id, moves);
                }
            }
        }
        pclose(pipe);
        std::cout << "[RECONNECTING] Stream lost, reconnecting in 2 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return 0;
}
