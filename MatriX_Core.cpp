#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include <chrono>

const std::string TOKEN = std::getenv("LICHESS_TOKEN") ? std::getenv("LICHESS_TOKEN") : "";
const std::vector<std::string> WHITELIST = {"muhammedeymengurbuz"}; 
bool isInGame = false;

bool isUserAllowed(std::string userId) {
    std::string lowerId = userId;
    std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
    for (const auto& user : WHITELIST) if (lowerId == user) return true;
    return false;
}

void sendMove(std::string gameId, std::string move) {
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\"";
    system((cmd + " > /dev/null 2>&1 &").c_str());
    std::cout << "[STRIKE] Move Sent: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves) {
    std::cout << "[THINKING] Stockfish is analyzing..." << std::endl << std::flush;
    std::string command = "(echo \"uci\"; echo \"isready\"; echo \"position startpos moves " + moves + "\"; echo \"go movetime 200\"; echo \"quit\") | stockfish 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[512];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("bestmove ") != std::string::npos) {
            std::stringstream ss(line);
            std::string tmp, best;
            ss >> tmp >> best;
            result = best;
            break;
        }
    }
    pclose(pipe);
    return result;
}

void handleGame(std::string gameId, bool amIWhite) {
    if(isInGame) return;
    isInGame = true;
    std::cout << "[GAME-LOCKED] ID: " << gameId << " | Playing as: " << (amIWhite ? "White" : "Black") << std::endl << std::flush;
    
    std::string cmd = "curl -s -N --keepalive-time 5 --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { isInGame = false; return; }

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.empty() || line == "\n") continue;

        size_t mPos = line.find("\"moves\":\"");
        if (mPos != std::string::npos) {
            size_t end = line.find("\"", mPos + 9);
            std::string allMoves = line.substr(mPos + 9, end - (mPos + 9));
            
            int moveCount = 0;
            std::stringstream ss(allMoves);
            std::string t;
            while (ss >> t) moveCount++;

            bool myTurn = (amIWhite && (moveCount % 2 == 0)) || (!amIWhite && (moveCount % 2 != 0));
            std::cout << "[STATUS] Moves: " << moveCount << " | MyTurn: " << (myTurn ? "YES" : "NO") << std::endl << std::flush;

            if (myTurn) {
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move != "bestmove" && move != "(none)") {
                    sendMove(gameId, move);
                }
            }
        }
        
        if (line.find("\"status\"") != std::string::npos && line.find("\"started\"") == std::string::npos) break;
    }
    pclose(pipe);
    isInGame = false;
    std::cout << "[SYSTEM] Game over. Ready for next target." << std::endl << std::flush;
}

void streamEvents() {
    std::string cmd = "curl -s -N --keepalive-time 5 --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t userPos = line.find("\"id\":\"", line.find("\"challenger\"")) + 6;
            std::string challengerId = line.substr(userPos, line.find("\"", userPos) - userPos);
            size_t c_id_pos = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(c_id_pos, line.find("\"", c_id_pos) - c_id_pos);
            
            if (isUserAllowed(challengerId)) {
                std::cout << "[AUTH] King Found: " << challengerId << ". Forcing Accept..." << std::endl << std::flush;
                std::string acceptCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\"";
                system((acceptCmd + " > /dev/null 2>&1").c_str());
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::thread(handleGame, c_id, false).detach(); 
            } else {
                std::string declineCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/decline\"";
                system((declineCmd + " > /dev/null 2>&1").c_str());
            }
        }
    }
    pclose(pipe);
}

int main() {
    if (TOKEN.empty()) { std::cerr << "TOKEN MISSING!" << std::endl; return 1; }
    std::cout << "[DEPLOY] MatriX_Core v8.3 DEMON-MODE Active." << std::endl << std::flush;
    while (true) {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
