import os
import requests
import json
import chess
import chess.engine
from groq import Groq

# Config
TOKEN = os.getenv("LICHESS_TOKEN")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}

# Llama 3 Client
client = Groq(api_key=GROQ_API_KEY)

def get_llama_response(message, context="chat"):
    """Llama 3.3 centilmen zekası."""
    try:
        prompts = {
            "welcome": "You are Matrix-Core, a polite chess AI. Say hello and wish luck briefly.",
            "chat": "You are Matrix-Core. Respond to the opponent's message in their language. Be brief and professional.",
            "win": "The game ended and you won. Congratulate the opponent on their good play humbly.",
            "loss": "The game ended and you lost. Sincerely congratulate the opponent on their victory."
        }
        
        completion = client.chat.completions.create(
            model="llama-3.3-70b-specdec",
            messages=[
                {"role": "system", "content": prompts.get(context, prompts["chat"])},
                {"role": "user", "content": message}
            ],
            temperature=0.7,
            max_tokens=60
        )
        return completion.choices[0].message.content
    except:
        return "Good game!" if context != "welcome" else "Hello! Good luck!"

def send_chat(game_id, message):
    url = f"https://lichess.org/api/bot/game/{game_id}/chat"
    data = {"room": "player", "text": message}
    try:
        requests.post(url, headers=HEADERS, data=data)
    except:
        pass

def get_best_move(moves_str):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split():
                board.push_uci(move)
        result = engine.play(board, chess.engine.Limit(time=1.5))
        engine.quit()
        return result.move.uci()
    except:
        return None

def handle_game(game_id):
    print(f"[SYSTEM] Starting Battle: {game_id}")
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False

    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    data = json.loads(line.decode('utf-8'))
                    
                    # 1. Welcome with Llama 3
                    if data.get("type") == "gameFull" and not welcome_sent:
                        msg = get_llama_response("Start of the game", context="welcome")
                        send_chat(game_id, msg)
                        welcome_sent = True

                    # 2. Chat Listener (Llama 3 responds to opponent)
                    if data.get("type") == "chatLine" and data.get("username") != "YOUR_BOT_USERNAME":
                        if data.get("room") == "player":
                            response_text = get_llama_response(data.get("text"))
                            send_chat(game_id, response_text)

                    # 3. Game State & Smart Endings
                    state = data.get("state", data)
                    moves = state.get("moves", "")
                    
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        winner = state.get("winner")
                        context = "win" if (winner == data.get("color")) else "loss"
                        final_msg = get_llama_response("Game over", context=context)
                        send_chat(game_id, final_msg)
                        break

                    # 4. Process Move
                    best_move = get_best_move(moves)
                    if best_move:
                        move_url = f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}"
                        requests.post(move_url, headers=HEADERS)
                except:
                    continue

def main():
    print("--- Matrix-Core v3.1: LLAMA 3 GENTLEMAN EDITION ---")
    event_url = "https://lichess.org/api/stream/event"
    with requests.get(event_url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        requests.post(f"https://lichess.org/api/challenge/{event['challenge']['id']}/accept", headers=HEADERS)
                    elif event.get("type") == "gameStart":
                        handle_game(event["game"]["id"])
                except:
                    continue

if __name__ == "__main__":
    main()
