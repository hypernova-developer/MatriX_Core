#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>

std::string get_best_move(std::string moves) {
    std::string command = "stockfish <<EOF\nposition startpos moves " + moves + "\ngo movetime 1000\nquit\nEOF";
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) return "error";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        if (line.find("bestmove") != std::string::npos) return line.substr(9, 4);
    }
    return "error";
}

void play_game(std::string game_id, std::string token) {
    std::string game_stream = "curl -s -N -L -H \"Authorization: Bearer " + token + "\" https://lichess.org/api/bot/game/stream/" + game_id;
    FILE* pipe = popen(game_stream.c_str(), "r");
    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("\"type\":\"gameFull\"") != std::string::npos || line.find("\"type\":\"gameState\"") != std::string::npos) {
            // Extract moves
            size_t move_pos = line.find("\"moves\":\"") + 9;
            size_t move_end = line.find("\"", move_pos);
            std::string current_moves = line.substr(move_pos, move_end - move_pos);
            
            std::string best_move = get_best_move(current_moves);
            if (best_move != "error") {
                std::string move_cmd = "curl -s -X POST -H \"Authorization: Bearer " + token + "\" https://lichess.org/api/bot/game/" + game_id + "/move/" + best_move;
                std::system(move_cmd.c_str());
            }
        }
    }
    pclose(pipe);
}

int main() {
    const char* t = std::getenv("LICHESS_TOKEN");
    if (!t) return 1;
    std::string token = std::string(t);
    std::cout << "Matrix-Core Combat Mode: Online" << std::endl;

    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + token + "\" https://lichess.org/api/stream/event";
    FILE* event_pipe = popen(stream_cmd.c_str(), "r");
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), event_pipe) != nullptr) {
        std::string event(buffer);
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_s = event.find("\"id\":\"") + 6;
            std::string c_id = event.substr(id_s, event.find("\"", id_s) - id_s);
            std::system(("curl -s -X POST -H \"Authorization: Bearer " + token + "\" https://lichess.org/api/challenge/" + c_id + "/accept").c_str());
        } else if (event.find("\"type\":\"gameStart\"") != std::string::npos) {
            size_t g_s = event.find("\"id\":\"") + 6;
            std::string g_id = event.substr(g_s, event.find("\"", g_s) - g_s);
            play_game(g_id, token); 
        }
    }
    pclose(event_pipe);
    return 0;
}
