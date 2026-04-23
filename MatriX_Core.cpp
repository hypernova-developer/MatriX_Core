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

void logError(std::string context, int code)
{
    if (code != 0)
    {
        std::cerr << "[!!! ERROR !!!] " << context << " failed with code: " << code << std::endl << std::flush;
    }
}

void sendMove(std::string gameId, std::string move)
{
    std::string cmd = "curl -s -X POST \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\" -H \"Authorization: Bearer " + TOKEN + "\"";
    int res = system(cmd.c_str());
    if (res != 0) logError("Move Execution (" + move + ")", res);
    else std::cout << "[MOVE] Strike successful: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves)
{
    std::string command = "(echo \"uci\"; echo \"isready\"; echo \"position startpos moves " + moves + "\"; echo \"go movetime 50\"; echo \"quit\") | stockfish 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) { logError("Stockfish Pipe", -1); return ""; }
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("bestmove ") != std::string::npos)
        {
            result = line.substr(9, 4);
            break;
        }
    }
    pclose(pipe);
    return result;
}

void handleGame(std::string gameId, bool amIWhite)
{
    std::cout << "[GAME-START] ID: " << gameId << " | Color: " << (amIWhite ? "White" : "Black") << std::endl << std::flush;
    std::string cmd = "curl -s -N -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { logError("Game Stream Pipe", -1); currentMatchesCount--; return; }

    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.length() < 5) continue;

        if (line.find("\"moves\"") != std::string::npos)
        {
            size_t pos = line.find("\"moves\":\"") + 9;
            std::string allMoves = line.substr(pos, line.find("\"", pos) - pos);
            
            size_t spaces = std::count(allMoves.begin(), allMoves.end(), ' ');
            bool myTurn = (allMoves.empty() && amIWhite) || (!allMoves.empty() && ((spaces + 1) % 2 == 0) == amIWhite);

            if (myTurn)
            {
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move.length() >= 4) sendMove(gameId, move);
            }
        }
        
        if (line.find("\"status\"") != std::string::npos && line.find("\"started\"") == std::string::npos)
        {
            std::cout << "[GAME-END] Status detected in stream." << std::endl << std::flush;
            break;
        }
    }
    pclose(pipe);
    currentMatchesCount--;
}

void streamEvents()
{
    std::cout << "[DEBUG] Opening Event Stream..." << std::endl << std::flush;
    std::string cmd = "curl -s -N -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { logError("Event Stream Pipe", -1); return; }

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.length() < 2) continue;

        std::cout << "[INCOMING] " << line << std::endl << std::flush;

        if (line.find("\"type\":\"challenge\"") != std::string::npos)
        {
            size_t idStart = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(idStart, line.find("\"", idStart) - idStart);
            
            std::string lowerLine = line;
            std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
            
            if (lowerLine.find("muhammedeymengurbuz") != std::string::npos)
            {
                std::cout << "[AUTH] Whitelisted user recognized. Sending Accept..." << std::endl << std::flush;
                std::string acceptCmd = "curl -s -X POST \"https://lichess.org/api/challenge/" + c_id + "/accept\" -H \"Authorization: Bearer " + TOKEN + "\"";
                int res = system(acceptCmd.c_str());
                if (res != 0) logError("Challenge Acceptance", res);
            }
            else
            {
                std::cout << "[AUTH] Stranger detected. Declining..." << std::endl << std::flush;
                std::string declineCmd = "curl -s -X POST \"https://lichess.org/api/challenge/" + c_id + "/decline\" -H \"Authorization: Bearer " + TOKEN + "\"";
                system(declineCmd.c_str());
            }
        }
        else if (line.find("\"type\":\"gameStart\"") != std::string::npos)
        {
            size_t gPos = line.find("\"id\":\"", line.find("\"game\"")) + 6;
            std::string g_id = line.substr(gPos, line.find("\"", gPos) - gPos);
            bool white = (line.find("\"color\":\"white\"") != std::string::npos);
            
            if (currentMatchesCount < 1)
            {
                currentMatchesCount++;
                std::thread(handleGame, g_id, white).detach();
            }
        }
    }
    int closeRes = pclose(pipe);
    logError("Event Stream Closed", closeRes);
}

int main()
{
    if (TOKEN.empty()) 
    {
        std::cerr << "[FATAL] LICHESS_TOKEN IS EMPTY! Check Secrets." << std::endl << std::flush;
        return 1;
    }

    std::cout << "[DEPLOY] MatriX_Core v7.5 Black-Box Active." << std::endl << std::flush;
    
    while (true)
    {
        streamEvents();
        std::cout << "[RETRY] Connection lost. Re-establishing in 5s..." << std::endl << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return 0;
}
