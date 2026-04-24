#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <poll.h>

const std::string STOCKFISH_PATH = "/usr/games/stockfish";
const char* rawToken = std::getenv("LICHESS_TOKEN");
const std::string TOKEN = rawToken ? rawToken : "";

const std::vector<std::string> BLACKLIST = 
{ 
    "scheunentor17", "socratesbot254", 
    "pzchessbot", "krausevich", "pat9471" 
};

struct EngineInstance 
{
    int inPipe[2], outPipe[2];
    pid_t pid = -1;

    void init() 
    {
        if (pid != -1) return;
        pipe(inPipe); 
        pipe(outPipe);
        pid = fork();
        if (pid == 0) 
        {
            dup2(inPipe[0], STDIN_FILENO);
            dup2(outPipe[1], STDOUT_FILENO);
            close(inPipe[1]); 
            close(outPipe[0]);
            execl(STOCKFISH_PATH.c_str(), "stockfish", (char*)NULL);
            _exit(1);
        }
        close(inPipe[0]); 
        close(outPipe[1]);
        std::string initCmd = "uci\nsetoption name Threads value 2\nsetoption name Hash value 256\nisready\n";
        write(inPipe[1], initCmd.c_str(), initCmd.length());
    }

    std::string getMove(const std::string& moves) 
    {
        std::string cmd = moves.empty() ? "position startpos\n" : "position startpos moves " + moves + "\n";
        cmd += "go movetime 50\n";
        write(inPipe[1], cmd.c_str(), cmd.length());

        std::string output;
        char buffer[4096];
        struct pollfd pfd = { outPipe[0], POLLIN, 0 };

        while (poll(&pfd, 1, 1000) > 0) 
        {
            ssize_t n = read(outPipe[0], buffer, sizeof(buffer) - 1);
            if (n <= 0) break;
            buffer[n] = '\0';
            output += buffer;
            if (output.find("bestmove") != std::string::npos) break;
        }

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

    void stop() 
    {
        if (pid == -1) return;
        write(inPipe[1], "quit\n", 5);
        close(inPipe[1]); 
        close(outPipe[0]);
        waitpid(pid, NULL, 0);
        pid = -1;
    }
} engine;

bool isBlacklisted(std::string userId) 
{
    std::transform(userId.begin(), userId.end(), userId.begin(), ::tolower);
    for (const auto& banned : BLACKLIST) 
    {
        if (userId == banned) return true;
    }
    return false;
}

void sendMove(const std::string& gameId, const std::string& move) 
{
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\" &";
    system(cmd.c_str());
    std::cout << "[STRIKE] Matrix Executed: " << move << std::endl << std::flush;
}

void handleGame(std::string gameId) 
{
    std::cout << "[LOCKED] Target Acquired: " << gameId << std::endl << std::flush;
    engine.init();
    std::string streamCmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/stream/" + gameId + "\"";
    FILE* stream = popen(streamCmd.c_str(), "r");
    if (!stream) return;

    bool amIWhite = false, colorFound = false;
    char buffer[16384];
    while (fgets(buffer, sizeof(buffer), stream)) 
    {
        std::string line(buffer);
        if (line.length() < 10) continue;

        if (!colorFound && line.find("\"white\":{\"id\":\"") != std::string::npos) 
        {
            size_t wPos = line.find("\"white\":{\"id\":\"") + 15;
            std::string whiteId = line.substr(wPos, line.find("\"", wPos) - wPos);
            std::transform(whiteId.begin(), whiteId.end(), whiteId.begin(), ::tolower);
            amIWhite = (whiteId == "matrix_core");
            colorFound = true;
            std::cout << "[INFO] Matrix is " << (amIWhite ? "WHITE" : "BLACK") << std::endl << std::flush;
        }

        if (line.find("\"moves\":\"") != std::string::npos) 
        {
            size_t mStart = line.find("\"moves\":\"") + 9;
            size_t mEnd = line.find("\"", mStart);
            std::string allMoves = (mStart == mEnd) ? "" : line.substr(mStart, mEnd - mStart);
            
            int count = 0;
            if (!allMoves.empty()) 
            {
                std::stringstream ss(allMoves);
                std::string t;
                while (ss >> t) count++;
            }

            if ((amIWhite && count % 2 == 0) || (!amIWhite && count % 2 != 0)) 
            {
                std::string bestMove = engine.getMove(allMoves);
                if (!bestMove.empty()) sendMove(gameId, bestMove);
            }
        }
        if (line.find("\"status\"") != std::string::npos && (line.find("\"mate\"") != std::string::npos || line.find("\"resign\"") != std::string::npos || line.find("\"draw\"") != std::string::npos)) break;
    }
    pclose(stream);
    engine.stop();
}

void streamEvents() 
{
    std::string cmd = "curl -s -N --no-buffer -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/stream/event\"";
    FILE* stream = popen(cmd.c_str(), "r");
    if (!stream) return;

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer), stream)) 
    {
        std::string line(buffer);
        if (line.find("\"type\":\"challenge\"") != std::string::npos) 
        {
            size_t idStart = line.find("\"id\":\"") + 6;
            std::string c_id = line.substr(idStart, line.find("\"", idStart) - idStart);
            size_t chObj = line.find("\"challenger\":{");
            if (chObj != std::string::npos) 
            {
                size_t nStart = line.find("\"id\":\"", chObj) + 6;
                std::string challengerId = line.substr(nStart, line.find("\"", nStart) - nStart);

                if (!isBlacklisted(challengerId)) 
                {
                    std::cout << "[ACCEPT] Challenge from: " << challengerId << std::endl << std::flush;
                    std::string acc = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\" &";
                    system(acc.c_str());
                    std::thread(handleGame, c_id).detach();
                } 
                else 
                {
                    std::cout << "[SECURITY] Declined (Blacklisted): " << challengerId << std::endl << std::flush;
                    std::string decline = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/decline\" &";
                    system(decline.c_str());
                }
            }
        }
    }
    pclose(stream);
}

int main() 
{
    if (TOKEN.empty()) return 1;
    std::cout << "[DEPLOY] MatriX_Core v7.18.0 Unnatural Disaster: Execution Demon" << std::endl << std::flush;
    while (true) 
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
