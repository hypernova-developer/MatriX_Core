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

bool isUserAllowed(std::string userId)
{
    std::string lowerId = userId;
    std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
    for (const auto& user : WHITELIST)
    {
        if (lowerId == user) return true;
    }
    return false;
}

void sendMove(std::string gameId, std::string move)
{
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\" > /dev/null 2>&1 &";
    system(cmd.c_str());
    std::cout << "[MOVE] Strike: " << move << std::endl << std::flush;
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
    std::cout << "[GAME-START] Target ID: " << gameId << std::endl << std::flush;
    
    std::string cmd = "curl -s -N --keepalive-time 10 --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { isInGame = false; return; }

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        size_t mPos = line.find("\"moves\":\"");
        if (mPos != std::string::npos)
        {
            size_t end = line.find("\"", mPos + 9);
            std::string allMoves = line.substr(mPos + 9, end - (mPos + 9));
            
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
            if (line.find("mate") != std::string::npos || line.find("resign") != std::string::npos || 
                line.find("draw") != std::string::npos || line.find("outoftime") != std::string::npos)
                break;
        }
    }
    pclose(pipe);
    isInGame = false;
    std::cout << "[GAME-END] Slot freed." << std::endl << std::flush;
}

void streamEvents()
{
    std::cout << "[DEBUG] Shield Active. Awaiting..." << std::endl << std::flush;
    std::string cmd = "curl -s -N --keepalive-time 10 --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos)
        {
            size_t userPos = line.find("\"id\":\"", line.find("\"challenger\"")) + 6;
            std::string challengerId = line.substr(userPos, line.find("\"", userPos) - userPos);
            
            size_t c_id_pos = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(c_id_pos, line.find("\"", c_id_pos) - c_id_pos);
            
            if (isUserAllowed(challengerId))
            {
                std::cout << "[AUTH] Whitelist user recognized: " << challengerId << ". Accepting..." << std::endl << std::flush;
                std::string acceptCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\" > /dev/null 2>&1";
                system(acceptCmd.c_str());
            }
            else
            {
                std::cout << "[SECURITY] Blocked challenge from: " << challengerId << std::endl << std::flush;
                std::string declineCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/decline\" > /dev/null 2>&1";
                system(declineCmd.c_str());
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
    std::cout << "[RECONNECT] Shield disrupted. Patching..." << std::endl << std::flush;
}

int main()
{
    if (TOKEN.empty()) return 1;
    std::cout << "[DEPLOY] MatriX_Core v8.2 Shield-Protocol Online." << std::endl << std::flush;
    while (true)
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
