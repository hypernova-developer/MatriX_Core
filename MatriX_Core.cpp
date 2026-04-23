#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include <chrono>
#include <fstream>

const std::string TOKEN = std::getenv("LICHESS_TOKEN") ? std::getenv("LICHESS_TOKEN") : "";
const std::vector<std::string> WHITELIST = {"muhammedeymengurbuz"};

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
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\"";
    system((cmd + " > /dev/null 2>&1 &").c_str());
    std::cout << "[STRIKE] Matrix Executed: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves)
{
    std::ofstream engineInput("input.txt");
    engineInput << "uci" << std::endl;
    engineInput << "isready" << std::endl;
    
    if (moves.empty()) 
    {
        engineInput << "position startpos" << std::endl;
    }
    else 
    {
        engineInput << "position startpos moves " << moves << std::endl;
    }
    
    engineInput << "go movetime 1000" << std::endl;
    engineInput << "quit" << std::endl;
    engineInput.close();

    FILE* pipe = popen("stockfish < input.txt 2>/dev/null", "r");
    if (!pipe) return "";

    char buffer[2048];
    std::string bestMove = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("bestmove ") != std::string::npos)
        {
            std::stringstream ss(line);
            std::string tag;
            ss >> tag >> bestMove;
            break;
        }
    }
    pclose(pipe);
    return bestMove;
}

void handleGame(std::string gameId)
{
    std::cout << "[LOCKED] Target Acquired: " << gameId << std::endl << std::flush;
    std::string streamCmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* pipe = popen(streamCmd.c_str(), "r");
    if (!pipe) return;

    bool amIWhite = false;
    bool colorFound = false;
    char buffer[16384];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        
        if (!colorFound && line.find("\"white\":{\"id\":\"") != std::string::npos)
        {
            size_t wPos = line.find("\"white\":{\"id\":\"") + 15;
            std::string whiteId = line.substr(wPos, line.find("\"", wPos) - wPos);
            std::transform(whiteId.begin(), whiteId.end(), whiteId.begin(), ::tolower);
            amIWhite = (whiteId.find("bot") != std::string::npos || whiteId.find("matrix") != std::string::npos);
            colorFound = true;
            std::cout << "[INFO] Matrix Identity: " << (amIWhite ? "WHITE" : "BLACK") << std::endl << std::flush;
        }

        if (line.find("\"moves\":\"") != std::string::npos)
        {
            size_t mStart = line.find("\"moves\":\"") + 9;
            size_t mEnd = line.find("\"", mStart);
            std::string allMoves = (mStart == mEnd) ? "" : line.substr(mStart, mEnd - mStart);
            
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
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move != "(none)" && move.length() >= 4)
                {
                    sendMove(gameId, move);
                }
            }
        }
        if (line.find("\"status\"") != std::string::npos && (line.find("\"mate\"") != std::string::npos || line.find("\"draw\"") != std::string::npos)) break;
    }
    pclose(pipe);
}

void streamEvents()
{
    std::string cmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos)
        {
            size_t cPos = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(cPos, line.find("\"", cPos) - cPos);
            
            std::string acceptCmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\"";
            system((acceptCmd + " > /dev/null 2>&1").c_str());
            
            std::thread(handleGame, c_id).detach();
        }
    }
    pclose(pipe);
}

int main()
{
    std::cout << "[DEPLOY] MatriX_Core v9.2 Unnatural Disaster: EXECUTION DEMON Online." << std::endl << std::flush;
    while (true)
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
