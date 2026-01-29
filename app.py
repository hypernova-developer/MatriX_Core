import os
import requests
import json
import chess
import chess.engine

# Config
TOKEN = os.getenv("LICHESS_TOKEN")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
AUTHORIZED_USER = "muhammedeymengurbuz" # Security Lock

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
        
        # 1.2 seconds of calculation time
        result = engine.play(board, chess.engine.Limit(time=1.2))
        engine.quit()
        return result.move.uci()
    except Exception as e:
        print(f"[ENGINE ERROR] {e}")
        return None

def handle_game(game_id):
    """Handles the live game stream and move execution."""
    print(f"[SYSTEM] Managing Game: {game_id}")
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False

    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    data = json.loads(line.decode('utf-8'))
                    
                    # 1. Greeting Sequence
                    if data.get("type") == "gameFull" and not welcome_sent:
                        send_chat(game_id, f"Hello {AUTHORIZED_USER}! Matrix-Core v2.4 Dev Mode is active. Good luck!")
                        welcome_sent = True

                    # 2. Game State & Move Management
                    state = data.get("state", data)
                    moves = state.get("moves", "")
                    
                    # Check for game end
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        send_chat(game_id, "Good game! Session terminated.")
                        print(f"[SYSTEM] Game {game_id} finished. Status: {state.get('status')}")
                        break

                    # 3. Process Engine Move
                    best_move = get_best_move(moves)
                    if best_move:
                        move_url = f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}"
                        move_res = requests.post(move_url, headers=HEADERS)
                        if move_res.status_code == 200:
                            print(f"[MOVE] {best_move} sent successfully.")
                except Exception as e:
                    continue

def main():
    print(f"--- Matrix-Core v2.4 Online ---")
    print(f"[INFO] Security Mode: Only accepting challenges from '{AUTHORIZED_USER}'")
    
    event_url = "https://lichess.org/api/stream/event"
    
    with requests.get(event_url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    event = json.loads(line.decode('utf-8'))
                    
                    # Challenge Management
                    if event.get("type") == "challenge":
                        challenger = event["challenge"]["challenger"]["id"]
                        challenge_id = event["challenge"]["id"]
                        
                        if challenger.lower() == AUTHORIZED_USER.lower():
                            requests.post(f"https://lichess.org/api/challenge/{challenge_id}/accept", headers=HEADERS)
                            print(f"[AUTH] Challenge ACCEPTED from: {challenger}")
                        else:
                            requests.post(f"https://lichess.org/api/challenge/{challenge_id}/decline", headers=HEADERS)
                            print(f"[SECURITY] Challenge DECLINED from unauthorized user: {challenger}")
                    
                    # Game Initialization
                    elif event.get("type") == "gameStart":
                        game_id = event["game"]["id"]
                        handle_game(game_id)
                except Exception as e:
                    continue

if __name__ == "__main__":
    main()
