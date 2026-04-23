#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <chrono>
#include <thread>

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
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\" > /dev/null 2>&1 &";
    system(cmd.c_str());
    std::cout << "[STRIKE] Matrix Executed: " << move << std::endl << std::flush;
}

std::string getBestMove(std::string moves)
{
    int inPipe[2], outPipe[2];
    if (pipe(inPipe) < 0 || pipe(outPipe) < 0) return "";

    pid_t pid = fork();
    if (pid == 0)
    {
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        close(inPipe[1]);
        close(outPipe[0]);

        char* argv[] = {(char*)"stockfish", NULL};
        execvp("stockfish", argv);
        exit(1);
    }

    close(inPipe[0]);
    close(outPipe[1]);

    std::string input = "uci\nisready\nposition startpos";
    if (!moves.empty()) input += " moves " + moves;
    input += "\ngo movetime 1000\nquit\n";

    write(inPipe[1], input.c_str(), input.length());
    close(inPipe[1]);

    char buffer[4096];
    std::string output = "";
    ssize_t bytesRead;
    while ((bytesRead = read(outPipe[0], buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytesRead] = '\0';
        output += buffer;
        if (output.find("bestmove") != std::string::npos) break;
    }
    close(outPipe[0]);
    waitpid(pid, NULL, 0);

    size_t pos = output.find("bestmove ");
    if (pos != std::string::npos)
    {
        std::stringstream ss(output.substr(pos + 9));
        std::string move;
        ss >> move;
        return move;
    }
    return "";
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
                std::string move = getBestMove(allMoves);
                if (!move.empty() && move != "(none)")
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
    std::cout << "[DEPLOY] MatriX_Core v11.0 Unnatural Disaster: EXECUTION DEMON Online." << std::endl << std::flush;
    while (true)
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
