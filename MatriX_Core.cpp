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
int currentMatchesCount = 0;

void sendMove(std::string gameId, std::string move)
{
    std::string cmd = "curl -s -X POST \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\" -H \"Authorization: Bearer " + TOKEN + "\"";
    system(cmd.c_str());
    std::cout << "[MOVE] Matrix strike: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves)
{
    std::string command = "(echo \"uci\"; echo \"isready\"; echo \"position startpos moves " + moves + "\"; echo \"go movetime 50\"; echo \"quit\") | stockfish 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        return "";
    }
    char buffer[128];
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
    std::cout << "[GAME-CONNECT] Entering room: " << gameId << std::endl << std::flush;
    std::string cmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        currentMatchesCount--;
        return;
    }

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.length() < 10)
        {
            continue;
        }

        if (line.find("\"moves\":\"") != std::string::npos)
        {
            size_t start = line.find("\"moves\":\"") + 9;
            size_t end = line.find("\"", start);
            std::string allMoves = line.substr(start, end - start);
            
            size_t spaces = 0;
            if(!allMoves.empty())
            {
                spaces = std::count(allMoves.begin(), allMoves.end(), ' ') + 1;
            }
            
            bool myTurn = (amIWhite && (spaces % 2 == 0)) || (!amIWhite && (spaces % 2 != 0));
            if (myTurn)
            {
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move.length() >= 4)
                {
                    sendMove(gameId, move);
                }
            }
        }
        if (line.find("\"status\"") != std::string::npos && line.find("\"started\"") == std::string::npos)
        {
            break;
        }
    }
    pclose(pipe);
    currentMatchesCount--;
    std::cout << "[SYSTEM] Game over: " << gameId << std::endl << std::flush;
}

void streamEvents()
{
    std::cout << "[DEBUG] Awaiting events..." << std::endl << std::flush;
    std::string cmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        return;
    }

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.length() < 5)
        {
            continue;
        }

        if (line.find("\"type\":\"challenge\"") != std::string::npos)
        {
            size_t idStart = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(idStart, line.find("\"", idStart) - idStart);
            
            if (line.find("muhammedeymengurbuz") != std::string::npos || line.find("Muhammedeymengurbuz") != std::string::npos)
            {
                std::cout << "[AUTH] Owner recognized. Accepting..." << std::endl << std::flush;
                std::string acceptCmd = "curl -s -X POST \"https://lichess.org/api/challenge/" + c_id + "/accept\" -H \"Authorization: Bearer " + TOKEN + "\"";
                system(acceptCmd.c_str());
            }
        }
        else if (line.find("\"type\":\"gameStart\"") != std::string::npos)
        {
            size_t gamePos = line.find("\"game\":");
            size_t idPos = line.find("\"id\":\"", gamePos) + 6;
            std::string g_id = line.substr(idPos, line.find("\"", idPos) - idPos);
            
            bool white = (line.find("\"color\":\"white\"") != std::string::npos);
            if (currentMatchesCount < 1)
            {
                currentMatchesCount++;
                std::thread(handleGame, g_id, white).detach();
            }
        }
    }
    pclose(pipe);
}

int main()
{
    if (TOKEN.empty())
    {
        return 1;
    }
    std::cout << "[DEPLOY] MatriX_Core v7.7 True-ID Online." << std::endl << std::flush;
    while (true)
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return 0;
}
