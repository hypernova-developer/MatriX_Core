#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>

std::string GLOBAL_TOKEN;

// Stockfish ile en iyi hamleyi bulma
std::string get_best_move(std::string moves) {
    std::ofstream engine_input("engine_in.txt");
    engine_input << "uci\nisready\nposition startpos moves " << moves << "\ngo movetime 1500\nquit\n";
    engine_input.close();

    std::array<char, 512> buffer;
    std::string result = "";
    FILE* pipe = popen("stockfish < engine_in.txt", "r");
    if (!pipe) return "error";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line = buffer.data();
        if (line.find("bestmove ") != std::string::npos) {
            result = line.substr(line.find("bestmove ") + 9, 4);
            break;
        }
    }
    pclose(pipe);
    return result;
}

// Oyunun içine girip hamleleri takip eden fonksiyon
void handle_game(std::string game_id) {
    std::cout << "[GAME] Infiltrating game: " << game_id << std::endl;
    std::string game_cmd = "curl -s -N -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/bot/game/stream/" + game_id;
    FILE* game_pipe = popen(game_cmd.c_str(), "r");
    if (!game_pipe) return;

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), game_pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("\"type\":\"gameState\"") != std::string::npos || line.find("\"type\":\"gameFull\"") != std::string::npos) {
            // Hamleleri ayıkla
            size_t m_pos = line.find("\"moves\":\"");
            if (m_pos != std::string::npos) {
                m_pos += 9;
                std::string current_moves = line.substr(m_pos, line.find("\"", m_pos) - m_pos);
                
                std::string best = get_best_move(current_moves);
                if (!best.empty() && best.length() >= 4) {
                    std::cout << "[MOVE] Thinking... Sending: " << best << std::endl;
                    std::string post_move = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                                          "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
                    std::system(post_move.c_str());
                }
            }
        }
    }
    pclose(game_pipe);
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    std::cout << "Matrix-Core v2.1: Infiltrator Active" << std::endl;

    while (true) {
        std::string event_cmd = "curl -s -N -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
        FILE* event_pipe = popen(event_cmd.c_str(), "r");
        if (!event_pipe) continue;

        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), event_pipe) != nullptr) {
            std::string event(buffer);
            
            // Meydan okuma kabulü
            if (event.find("\"type\":\"challenge\"") != std::string::npos) {
                size_t id_p = event.find("\"id\":\"") + 6;
                std::string c_id = event.substr(id_p, 8);
                std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
                std::cout << "[EVENT] Accepted: " << c_id << std::endl;
            }
            // Oyun başladığında içeri sızma
            else if (event.find("\"type\":\"gameStart\"") != std::string::npos) {
                size_t g_p = event.find("\"id\":\"") + 6;
                if (g_p < 6) g_p = event.find("\"gameId\":\"") + 10;
                std::string g_id = event.substr(g_p, 8);
                
                // Oyunu takip etmeye başla
                handle_game(g_id);
            }
        }
        pclose(event_pipe);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
