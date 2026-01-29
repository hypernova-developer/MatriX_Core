#include <iostream>
#include <string>
#include <cstdio>
#include <memory>

void execute_command(const std::string& cmd) {
    std::system(cmd.c_str());
}

int main() {
    const char* token_ptr = std::getenv("LICHESS_TOKEN");
    if (!token_ptr) return 1;
    std::string token = std::string(token_ptr);

    std::cout << "Matrix-Core Battle Protocol: Initialized" << std::endl;

    std::string stream_cmd = "curl -s -N -L -H \"Authorization: Bearer " + token + "\" https://lichess.org/api/stream/event";
    
    FILE* pipe = popen(stream_cmd.c_str(), "r");
    if (!pipe) return 1;

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string event(buffer);
        
        if (event.find("\"type\":\"challenge\"") != std::string::npos) {
            size_t id_start = event.find("\"id\":\"") + 6;
            size_t id_end = event.find("\"", id_start);
            std::string challenge_id = event.substr(id_start, id_end - id_start);
            
            std::cout << "Challenge detected! ID: " << challenge_id << std::endl;
            
            std::string accept_cmd = "curl -s -X POST -H \"Authorization: Bearer " + token + 
                                   "\" https://lichess.org/api/challenge/" + challenge_id + "/accept";
            execute_command(accept_cmd);
            std::cout << "Response sent to Lichess." << std::endl;
        }
    }

    pclose(pipe);
    return 0;
}
