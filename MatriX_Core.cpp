#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include <chrono>

const std::string TOKEN = std::getenv("LICHESS_TOKEN") ? std::getenv("LICHESS_TOKEN") : "";
const std::string MY_USERNAME = "Muhammedeymengurbuz";
const std::vector<std::string> WHITELIST = { "Muhammedeymengurbuz" };
int currentMatchesCount = 0;

bool isWhitelisted(std::string username)
{
    if (username.empty()) return false;
    
    std::string lowerUser = username;
    std::transform(lowerUser.begin(), lowerUser.end(), lowerUser.begin(), ::tolower);
    
    for (const auto& user : WHITELIST)
    {
        std::string lowerWhite = user;
        std::transform(lowerWhite.begin(), lowerWhite.end(), lowerWhite.begin(), ::tolower);
        if (lowerWhite == lowerUser)
        {
            return true;
        }
    }
    return false;
}

void sendMove(std::string gameId, std::string move)
{
    if (gameId.empty() || move.empty()) return;
    
    std::string cmd = "curl -s -X POST https://lichess.org/api/bot/game/" + gameId + "/move/" + move + " -H \"Authorization: Bearer " + TOKEN + "\" > /dev/null";
    system(cmd.c_str());
    std::cout << "[MOVE] Matrix executed strike: " << move << std::endl;
}

std::string getBestMove(std::string moves)
{
    std::string command = "(echo \"uci\"; echo \"isready\"; echo \"position startpos moves " + moves + "\"; echo \"go movetime 50\"; echo \"quit\") | stockfish 2>/dev/null";
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    
    if (!pipe) 
    {
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        std::string line(buffer.data());
        if (line.find("bestmove ") != std::string::npos)
        {
            std::string foundMove = line.substr(9, 4);
            if (foundMove != "(non") return foundMove;
        }
    }
    return "";
}

void handleGame(std::string gameId, bool amIWhite)
{
    std::string cmd = "curl -s -H \"Authorization: Bearer " + TOKEN + "\" https://lichess.org/api/bot/game/stream/" + gameId;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    
    if (!pipe) 
    {
        currentMatchesCount--;
        return;
    }

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
    {
        std::string line(buffer);
        
        if (line.find("\"status\":\"") != std::string::npos && line.find("\"status\":\"started\"") == std::string::npos)
        {
            break;
        }

        size_t movesPos = line.find("\"moves\":\"");
        if (movesPos != std::string::npos)
        {
            size_t start = movesPos + 9;
            std::string allMoves = line.substr(start, line.find("\"", start) - start);
            
            size_t spaceCount = std::count(allMoves.begin(), allMoves.end(), ' ');
            bool isMyTurn = (allMoves.empty() && amIWhite) || 
                            (!allMoves.empty() && ((spaceCount + 1) % 2 == 0) == amIWhite);

            if (isMyTurn)
            {
                std::string nextMove = getBestMove(allMoves);
                if (!nextMove.empty())
                {
                    sendMove(gameId, nextMove);
                }
            }
        }
    }
    
    currentMatchesCount--;
    std::cout << "[SYSTEM] Game slot released." << std::endl;
}

void processChallenge(std::string challengerId, std::string challengeId)
{
    if (isWhitelisted(challengerId) && currentMatchesCount < 1)
    {
        std::string cmd = "curl -s -X POST https://lichess.org/api/challenge/" + challengeId + "/accept -H \"Authorization: Bearer " + TOKEN + "\" > /dev/null";
        system(cmd.c_str());
        std::cout << "[SYSTEM] Whitelist match accepted: " << challengerId << std::endl;
    }
    else
    {
        std::string cmd = "curl -s -X POST https://lichess.org/api/challenge/" + challengeId + "/decline -H \"Authorization: Bearer " + TOKEN + "\" > /dev/null";
        system(cmd.c_str());
    }
}

void streamEvents()
{
    std::string cmd = "curl -s -H \"Authorization: Bearer " + TOKEN + "\" https://lichess.org/api/stream/event";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    
    if (!pipe) return;

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
    {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos)
        {
            size_t idPos = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(idPos, line.find("\"", idPos) - idPos);
            size_t userPos = line.find("\"id\":\"", line.find("\"challenger\"")) + 6;
            std::string c_user = line.substr(userPos, line.find("\"", userPos) - userPos);
            
            processChallenge(c_user, c_id);
        }
        else if (line.find("\"type\":\"gameStart\"") != std::string::npos && currentMatchesCount < 1)
        {
            size_t gameIdPos = line.find("\"id\":\"", line.find("\"game\"")) + 6;
            std::string g_id = line.substr(gameIdPos, line.find("\"", gameIdPos) - gameIdPos);
            
            bool amIWhite = (line.find("\"color\":\"white\"") != std::string::npos);
            
            currentMatchesCount++;
            std::thread(handleGame, g_id, amIWhite).detach();
        }
    }
}

int main()
{
    if (TOKEN.empty())
    {
        std::cerr << "[FATAL] LICHESS_TOKEN missing in Environment Variables!" << std::endl;
        return 1;
    }

    std::cout << "[DEPLOY] MatriX_Core v7.1 IRONCLAD Active." << std::endl;

    while (true)
    {
        try
        {
            streamEvents();
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    return 0;
}
