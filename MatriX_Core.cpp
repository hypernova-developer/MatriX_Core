#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <chrono>
#include <thread>

const std::string STOCKFISH_PATH = "/usr/games/stockfish";
const std::vector<std::string> WHITELIST = { "muhammedeymengurbuz" };
const char* rawToken = std::getenv("LICHESS_TOKEN");
const std::string TOKEN = rawToken ? rawToken : "";

bool isUserAllowed(std::string userId)
{
    std::transform(userId.begin(), userId.end(), userId.begin(), ::tolower);
    for (const auto& allowed : WHITELIST)
    {
        if (userId == allowed) return true;
    }
    return false;
}

void sendMove(std::string gameId, std::string move)
{
    std::string cmd = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/bot/game/" + gameId + "/move/" + move + "\"";
    system((cmd + " > /dev/null 2>&1").c_str());
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
        execl(STOCKFISH_PATH.c_str(), "stockfish", (char*)NULL);
        _exit(1);
    }

    close(inPipe[0]);
    close(outPipe[1]);

    std::stringstream input;
    input << "uci\nsetoption name Threads value 2\nsetoption name Hash value 128\nisready\n";
    if (moves.empty())
        input << "position startpos\n";
    else
        input << "position startpos moves " << moves << "\n";
    
    input << "go movetime 50\n";

    std::string s = input.str();
    write(inPipe[1], s.c_str(), s.length());

    char buffer[16384];
    std::string output = "";
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
    {
        ssize_t n = read(outPipe[0], buffer, sizeof(buffer) - 1);
        if (n <= 0) break;
        buffer[n] = '\0';
        output += buffer;
        if (output.find("bestmove") != std::string::npos) break;
    }

    write(inPipe[1], "quit\n", 5);
    close(inPipe[1]);
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
            
            if (whiteId == "matrix_core") amIWhite = true;
            else amIWhite = false;

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
                std::string bestMove = getBestMove(allMoves);
                if (!bestMove.empty()) sendMove(gameId, bestMove);
            }
        }
        if (line.find("\"status\"") != std::string::npos && (line.find("\"mate\"") != std::string::npos || line.find("\"resign\"") != std::string::npos)) break;
    }
    pclose(stream);
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

                if (isUserAllowed(challengerId))
                {
                    std::string acc = "curl -s -X POST -H \"Authorization: Bearer " + TOKEN + "\" \"https://lichess.org/api/challenge/" + c_id + "/accept\"";
                    system((acc + " > /dev/null 2>&1").c_str());
                    std::thread(handleGame, c_id).detach();
                }
            }
        }
    }
    pclose(stream);
}

int main()
{
    if (TOKEN.empty()) return 1;
    std::cout << "[DEPLOY] MatriX_Core v7.17.0 Unnatural Disaster: EXECUTION DEMON" << std::endl << std::flush;
    while (true)
    {
        streamEvents();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
