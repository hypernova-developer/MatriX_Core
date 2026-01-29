#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <vector>

void execute_command(const std::string& cmd) {
    std::system(cmd.c_str());
}

int main() {
    const char* token_ptr = std::getenv("LICHESS_TOKEN");
    if (!token_ptr) return 1;
    std::string token = std::string(token_ptr);

    std::cout << "Matrix-Core Battle Protocol: Initialized" << std::endl;

    // Listen for events (Challenges, Game Starts etc.)
    std::string stream_cmd = "curl -s -H \"Authorization: Bearer " + token + "\" https://lichess.org/api/stream/event";
    
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    if (!pipe) return 1;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        
        // Check if it's a challenge
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_pos = event.find("\"id\":\"");
            if (id_pos != std::string::npos) {
                std::string challenge_id = event.substr(id_pos + 6, 8); // Extracting 8-char ID
                std::cout << "Challenge detected! ID: " << challenge_id << std::endl;
                
                // Accept the challenge
                std::string accept_cmd = "curl -s -X POST -H \"Authorization: Bearer " + token + 
                                       "\" https://lichess.org/api/challenge/" + challenge_id + "/accept";
                execute_command(accept_cmd);
                std::cout << "Challenge accepted." << std::endl;
            }
        }
    }

    pclose(pipe);
    return 0;
}
