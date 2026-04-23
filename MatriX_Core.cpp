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
    std::cout << "[STRIKE] Matrix executed: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves) {
    std::string command = "(echo \"uci\"; echo \"isready\"; echo \"position startpos moves " + moves + "\"; echo \"go movetime 500\") | stockfish 2>/dev/null | grep \"bestmove\"";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[512];
    std::string result = "";
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("bestmove ") != std::string::npos) {
            std::stringstream ss(line);
            std::string tag, move;
            ss >> tag >> move;
            result = move;
        }
    }
    pclose(pipe);
    
    return result;
}

void handleGame(std::string gameId, bool amIWhite) {
    if(isInGame) return;
    isInGame = true;
    std::cout << "[TARGET-LOCKED] Analyzing ID: " << gameId << std::endl << std::flush;
    
    std::string cmd = "curl -s -N --keepalive-time 5 --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { isInGame = false; return; }

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        size_t mPos = line.find("\"moves\":\"");
        if (mPos != std::string::npos) {
            size_t mStart = mPos + 9;
            size_t mEnd = line.find("\"", mStart);
            std::string allMoves = line.substr(mStart, mEnd - mStart);
            
            int moveCount = 0;
            std::stringstream ss(allMoves);
            std::string t;
            while (ss >> t) moveCount++;

            bool myTurn = (amIWhite && (moveCount % 2 == 0)) || (!amIWhite && (moveCount % 2 != 0));

            if (myTurn) {
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move != "(none)") {
                    sendMove(gameId, move);
                } else {
                    std::cout << "[ERROR] Engine failed to generate move!" << std::endl << std::flush;
                }
            }
        }
        if (line.find("\"status\"") != std::string::npos && line.find("\"started\"") == std::string::npos) break;
    }
    pclose(pipe);
    isInGame = false;
    std::cout << "[SYSTEM] Neutralized." << std::endl << std::flush;
}

void streamEvents() {
    std::string cmd = "curl -s -N --keepalive-time 5 --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t uPos = line.find("\"id\":\"", line.find("\"challenger\"")) + 6;
            std::string challengerId = line.substr(uPos, line.find("\"", uPos) - uPos);
            size_t cPos = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(cPos, line.find("\"", cPos) - cPos);
            
            if (isUserAllowed(challengerId)) {
                std::cout << "[AUTH] King Access: " << challengerId << std::endl << std::flush;
                std::string acceptCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\"";
                system((acceptCmd + " > /dev/null 2>&1").c_str());
                
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                std::thread(handleGame, c_id, false).detach(); 
            }
        }
    }
    pclose(pipe);
}

int main() {
    if (TOKEN.empty()) return 1;
    std::cout << "[DEPLOY] MatriX_Core v8.6 EXECUTOR Online." << std::endl << std::flush;
    while (true) {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
