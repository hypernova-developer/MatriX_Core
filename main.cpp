#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    std::ofstream engine_input("engine_in.txt");
    engine_input << "uci\n" << "isready\n" << "position startpos moves " << moves << "\n" << "go movetime 1500\n" << "quit\n";
    engine_input.close();

    std::array<char, 512> buffer;
    std::string result = "";
    FILE* pipe = popen("stockfish < engine_in.txt", "r");
    if (!pipe) return "error";
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line = buffer.data();
        size_t pos = line.find("bestmove ");
        if (pos != std::string::npos) {
            result = line.substr(pos + 9, 4);
            break;
        }
    }
    pclose(pipe);
    return result;
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    std::cout << "Matrix-Core v2.0: Grandmaster Evolution" << std::endl;

    while (true) {
        std::string stream_cmd = "curl -s -N -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
        FILE* pipe = popen(stream_cmd.c_str(), "r");
        if (!pipe) continue;

        char buffer[16384]; // Buffer expanded for long move strings
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string event(buffer);
            if (event.length() < 5) continue;

            // 1. CHALLENGE HANDLER
            if (event.find("\"type\":\"challenge\"") != std::string::npos) {
                size_t id_pos = event.find("\"id\":\"") + 6;
                std::string c_id = event.substr(id_pos, 8);
                std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
                std::cout << "[SYSTEM] Accepted: " << c_id << std::endl;
            }

            // 2. MOVE HANDLER (Refined)
            size_t move_idx = event.find("\"moves\":\"");
            if (move_idx != std::string::npos) {
                // Find Game ID in the same or previous block
                size_t g_id_pos = event.find("\"id\":\"");
                if (g_id_pos == std::string::npos) g_id_pos = event.find("\"gameId\":\"");
                
                if (g_id_pos != std::string::npos) {
                    // Extract ID safely
                    std::string g_id = "";
                    for(int i=0; i<20; i++) { // Look ahead for the 8-char ID
                        std::string sub = event.substr(g_id_pos + i, 8);
                        if(sub.find("\"") == std::string::npos && sub.find(":") == std::string::npos) {
                            g_id = sub;
                            break;
                        }
                    }

                    if (!g_id.empty() && g_id.find("muham") == std::string::npos) {
                        std::string moves_str = event.substr(move_idx + 9, event.find("\"", move_idx + 9) - (move_idx + 9));
                        std::string best = get_best_move(moves_str);
                        
                        if (best.length() >= 4 && best.find("error") == std::string::npos) {
                            std::cout << "[BATTLE] Playing " << best << " in " << g_id << std::endl;
                            std::system(("curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/bot/game/" + g_id + "/move/" + best).c_str());
                        }
                    }
                }
            }
        }
        pclose(pipe);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
