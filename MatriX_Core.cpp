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
std::string lastAcceptedId = "";
bool isInGame = false;

void sendMove(std::string gameId, std::string move)
{
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\" > /dev/null &";
    system(cmd.c_str());
    std::cout << "[MOVE] Matrix strike: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves)
{
    std::string command = "(echo \"uci\"; echo \"isready\"; echo \"position startpos moves " + moves + "\"; echo \"go movetime 150\"; echo \"quit\") | stockfish 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("bestmove ") != std::string::npos)
        {
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

void handleGame(std::string gameId, bool amIWhite)
{
    isInGame = true;
    std::cout << "[GAME-CONNECT] Target: https://lichess.org/" << gameId << std::endl << std::flush;
    
    std::string cmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) 
    {
        isInGame = false;
        return;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("\"moves\":\"") != std::string::npos)
        {
            size_t start = line.find("\"moves\":\"") + 9;
            size_t end = line.find("\"", start);
            std::string allMoves = line.substr(start, end - start);
            
            int moveCount = 0;
            if (!allMoves.empty())
            {
                std::stringstream ss(allMoves);
                std::string t;
                while (ss >> t) moveCount++;
            }

            bool myTurn = (amIWhite && (moveCount % 2 == 0)) || (!amIWhite && (moveCount % 2 != 0));
            if (myTurn)
            {
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move.length() >= 4 && move != "bestmove")
                {
                    sendMove(gameId, move);
                }
            }
        }
        
        if (line.find("\"status\"") != std::string::npos && line.find("\"started\"") == std::string::npos)
        {
            if (line.find("mate") != std::string::npos || line.find("resign") != std::string::npos || line.find("draw") != std::string::npos)
                break;
        }
    }
    pclose(pipe);
    isInGame = false;
    std::cout << "[SYSTEM] Dead-end. Slot cleared." << std::endl << std::flush;
}

void streamEvents()
{
    std::string cmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos)
        {
            size_t idPos = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(idPos, line.find("\"", idPos) - idPos);
            
            if (c_id != lastAcceptedId)
            {
                std::cout << "[AUTH] Challenge Detected: " << c_id << std::endl << std::flush;
                std::string acceptCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\"";
                system(acceptCmd.c_str());
                lastAcceptedId = c_id;
            }
        }
        else if (line.find("\"type\":\"gameStart\"") != std::string::npos)
        {
            size_t gPos = line.find("\"game\":");
            size_t idPos = line.find("\"id\":\"", gPos) + 6;
            std::string g_id = line.substr(idPos, line.find("\"", idPos) - idPos);
            
            bool white = (line.find("\"color\":\"white\"") != std::string::npos);
            
            if (!isInGame && g_id.length() < 12) 
            {
                std::thread(handleGame, g_id, white).detach();
            }
        }
    }
    pclose(pipe);
}

int main()
{
    if (TOKEN.empty()) return 1;
    std::cout << "[DEPLOY] MatriX_Core v8.0 Heavy-Duty Active." << std::endl << std::flush;
    while (true)
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}
