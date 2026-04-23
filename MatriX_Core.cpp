#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <array>
#include <cstdio>
#include <cstdlib>

const std::string TOKEN = std::getenv("LICHESS_TOKEN") ? std::getenv("LICHESS_TOKEN") : "";
const std::string MY_USERNAME = "Muhammedeymengurbuz";
const std::vector<std::string> WHITELIST = { "Muhammedeymengurbuz" };

bool isWhitelisted(std::string username)
{
    for (const auto& user : WHITELIST)
    {
        if (user == username)
        {
            return true;
        }
    }
    return false;
}

std::string exec(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
    {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    return result;
}

void processChallenge(std::string challengerId, std::string challengeId)
{
    if (isWhitelisted(challengerId))
    {
        std::string cmd = "curl -X POST https://lichess.org/api/challenge/" + challengeId + "/accept -H \"Authorization: Bearer " + TOKEN + "\"";
        system(cmd.c_str());
        std::cout << "[SYSTEM] Challenge accepted: " << challengerId << std::endl;
    }
    else
    {
        std::string cmd = "curl -X POST https://lichess.org/api/challenge/" + challengeId + "/decline -H \"Authorization: Bearer " + TOKEN + "\"";
        system(cmd.c_str());
        std::cout << "[SYSTEM] Unauthorized challenge declined: " << challengerId << std::endl;
    }
}

std::string getStockfishMove(std::string moves)
{
    FILE* pipe = popen("stockfish", "w");
    if (!pipe)
    {
        pipe = popen("/usr/games/stockfish", "w");
    }

    if (pipe)
    {
        fprintf(pipe, "uci\n");
        fprintf(pipe, "isready\n");
        fprintf(pipe, "position startpos moves %s\n", moves.c_str());
        fprintf(pipe, "go movetime 100\n");
        fprintf(pipe, "quit\n");
        pclose(pipe);
    }
    
    return ""; 
}

void streamEvents()
{
    std::string cmd = "curl -s -H \"Authorization: Bearer " + TOKEN + "\" https://lichess.org/api/stream/event";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    
    if (!pipe)
    {
        return;
    }

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
    }
}

int main()
{
    if (TOKEN.empty())
    {
        std::cerr << "[ERROR] LICHESS_TOKEN not found in environment variables!" << std::endl;
        return 1;
    }

    std::cout << "[START] MatriX_Core C++ Deployment Active." << std::endl;
    std::cout << "[CONFIG] Whitelist Protection: ENABLED." << std::endl;

    while (true)
    {
        try
        {
            streamEvents();
        }
        catch (...)
        {
            std::cout << "[RETRY] Connection lost. Reconnecting in 5s..." << std::endl;
        }
    }

    return 0;
}
