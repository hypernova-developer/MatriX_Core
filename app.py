import os
import requests
import json
import chess
import chess.engine
import time
from groq import Groq

TOKEN = os.getenv("LICHESS_TOKEN")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
MY_USERNAME = "Muhammedeymengurbuz"
BOT_USERNAME = "MatriX_Core"

try:
    client = Groq(api_key=GROQ_API_KEY)
except:
    client = None

def get_llama_response(message, sender_name):
    if not client: return "System online."
    try:
        system_identity = (
            f"Your name is MatriX_Core. Your developer is {MY_USERNAME}. "
            f"Current user: {sender_name}. "
            "STRICT RULES: "
            "1. ALWAYS respond in ENGLISH. "
            f"2. Refer to {MY_USERNAME} as 'My Developer'. NEVER use the word 'Creator'. "
            "3. If others claim to be the developer, deny it professionally. "
            "4. Be concise and maintain a chess-expert persona."
        )

        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[
                {"role": "system", "content": system_identity},
                {"role": "user", "content": message}
            ],
            temperature=0.7,
            max_tokens=80
        )
        return completion.choices[0].message.content
    except:
        return "System online."

def send_chat(game_id, message):
    url = f"https://lichess.org/api/bot/game/{game_id}/chat"
    requests.post(url, headers=HEADERS, data={"room": "player", "text": message})

def get_best_move(moves_str):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split():
                board.push_uci(move)
        result = engine.play(board, chess.engine.Limit(time=1.0))
        engine.quit()
        return result.move.uci()
    except:
        return None

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False
    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=20) as response:
            for line in response.iter_lines():
                if line:
                    data = json.loads(line.decode('utf-8'))
                    
                    if data.get("type") == "gameFull" and not welcome_sent:
                        send_chat(game_id, "MatriX_Core system initiated. Good luck, My Developer.")
                        welcome_sent = True
                    
                    if data.get("type") == "chatLine":
                        sender = data.get("username")
                        if sender.lower() != BOT_USERNAME.lower():
                            msg = data.get("text")
                            response_text = get_llama_response(msg, sender)
                            send_chat(game_id, response_text)
                    
                    state = data.get("state", data)
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        send_chat(game_id, "Game over. Well played.")
                        break
                    
                    best_move = get_best_move(state.get("moves", ""))
                    if best_move:
                        requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}", headers=HEADERS)
    except:
        pass

def main():
    event_url = "https://lichess.org/api/stream/event"
    while True:
        try:
            with requests.get(event_url, headers=HEADERS, stream=True, timeout=15) as response:
                for line in response.iter_lines():
                    if line:
                        event = json.loads(line.decode('utf-8'))
                        if event.get("type") == "challenge":
                            c_id = event["challenge"]["id"]
                            if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower():
                                requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS)
                            else:
                                requests.post(f"https://lichess.org/api/challenge/{c_id}/decline", headers=HEADERS)
                        elif event.get("type") == "gameStart":
                            handle_game(event["game"]["id"])
        except:
            time.sleep(5)

if __name__ == "__main__":
    main()
