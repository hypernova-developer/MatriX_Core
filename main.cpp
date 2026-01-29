#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>

std::string GLOBAL_TOKEN;

std::string get_best_move(std::string moves) {
    // Write UCI commands to a temporary file for Stockfish to read reliably
    std::ofstream engine_input("engine_in.txt");
    engine_input << "uci\n";
    engine_input << "isready\n";
    engine_input << "position startpos moves " << moves << "\n";
    engine_input << "go movetime 2000\n";
    engine_input << "quit\n";
    engine_input.close();

    std::string command = "stockfish < engine_in.txt";
    std::array<char, 512> buffer;
    std::string result = "";
    
    FILE* pipe = popen(command.c_str(), "r");
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

void make_move(std::string game_id, std::string moves) {
    // Check if it's actually our turn (Simulated: Lichess handles this, but we log it)
    std::string best = get_best_move(moves);
    
    if (!best.empty() && best != "error" && best.find("(") == std::string::npos) {
        std::cout << "[GAME] ID: " << game_id << " | Calculated Move: " << best << std::endl;
        std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + 
                               "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best;
        
        // Execute move and capture response to keep connection alive
        FILE* curl_pipe = popen(move_cmd.c_str(), "r");
        char response[1024];
        if (fgets(response, sizeof(response), curl_pipe)) {
            std::cout << "[LICHESS] Response: " << response << std::endl;
        }
        pclose(curl_pipe);
    }
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    GLOBAL_TOKEN = std::string(t);
    
    std::cout << "Matrix-Core v1.9: Unstoppable Mode" << std::endl;

    while (true) {
        std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/stream/event";
        FILE* pipe = popen(stream_cmd.c_str(), "r");
        if (!pipe) continue;

        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string event(buffer);
            
            if (event.find("\"type\":\"challenge\"") != std::string::npos) {
                size_t id_s = event.find("\"id\":\"") + 6;
                std::string c_id = event.substr(id_s, 8);
                std::string accept_cmd = "curl -s -X POST -H \"Authorization: Bearer " + GLOBAL_TOKEN + "\" https://lichess.org/api/challenge/" + c_id + "/accept";
                std::system(accept_cmd.c_str());
                std::cout << "[EVENT] Challenge Accepted: " << c_id << std::endl;
            } 
            else if (event.find("\"moves\"") != std::string::npos) {
                size_t g_pos = event.find("\"id\":\"");
                if (g_pos == std::string::npos) g_pos = event.find("\"gameId\":\"");
                size_t start = (event.find("gameId") != std::string::npos) ? g_pos + 10 : g_pos + 6;
                std::string g_id = event.substr(start, 8);

                if (g_id.find("muham") == std::string::npos) {
                    size_t m_s = event.find("\"moves\":\"") + 9;
                    std::string moves = event.substr(m_s, event.find("\"", m_s) - m_s);
                    make_move(g_id, moves);
                }
            }
        }
        pclose(pipe);
        std::cout << "[SYSTEM] Reconnecting stream..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
