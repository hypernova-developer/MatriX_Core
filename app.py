import os
import requests
import json
import chess
import chess.engine

# Config
TOKEN = os.getenv("LICHESS_TOKEN")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}

def send_chat(game_id, message):
    """Sends a chat message to the game room."""
    url = f"https://lichess.org/api/bot/game/{game_id}/chat"
    data = {"room": "player", "text": message}
    try:
        requests.post(url, headers=HEADERS, data=data)
    except Exception as e:
        print(f"[ERROR] Failed to send chat: {e}")

def get_best_move(moves_str):
    """Calculates the best move using Stockfish engine."""
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split():
                board.push_uci(move)
        
        # 1.5 seconds of calculation for higher quality moves
        result = engine.play(board, chess.engine.Limit(time=1.5))
        engine.quit()
        return result.move.uci()
    except Exception as e:
        print(f"[ENGINE ERROR] {e}")
        return None

def handle_game(game_id):
    """Handles the live game stream and move execution."""
    print(f"[SYSTEM] Starting Battle: {game_id}")
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False

    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    data = json.loads(line.decode('utf-8'))
                    
                    # 1. Dynamic Greeting Sequence
                    if data.get("type") == "gameFull" and not welcome_sent:
                        white_name = data["white"].get("name", "Player")
                        black_name = data["black"].get("name", "Player")
                        # Greetings to the human opponent
                        send_chat(game_id, f"Hello! I am Matrix-Core v3.0. Good luck to everyone!")
                        welcome_sent = True

                    # 2. Game State Management
                    state = data.get("state", data)
                    moves = state.get("moves", "")
                    
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        send_chat(game_id, "Good game! Thanks for the match.")
                        print(f"[SYSTEM] Game Over: {game_id}")
                        break

                    # 3. Process Move
                    best_move = get_best_move(moves)
                    if best_move:
                        move_url = f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}"
                        requests.post(move_url, headers=HEADERS)
                except Exception:
                    continue

def main():
    print("--- Matrix-Core v3.0: GLOBAL RELEASE ONLINE ---")
    print("[STATUS] Listening for challenges from all users...")
    
    event_url = "https://lichess.org/api/stream/event"
    
    with requests.get(event_url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    event = json.loads(line.decode('utf-8'))
                    
                    # Challenge Acceptance (Unlocked)
                    if event.get("type") == "challenge":
                        challenge_id = event["challenge"]["id"]
                        challenger_name = event["challenge"]["challenger"]["name"]
                        
                        # Accept all challenges automatically
                        requests.post(f"https://lichess.org/api/challenge/{challenge_id}/accept", headers=HEADERS)
                        print(f"[EVENT] Accepted challenge from: {challenger_name}")
                    
                    elif event.get("type") == "gameStart":
                        game_id = event["game"]["id"]
                        handle_game(game_id)
                except Exception:
                    continue

if __name__ == "__main__":
    main()
