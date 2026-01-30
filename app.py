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

try:
    client = Groq(api_key=GROQ_API_KEY)
except Exception as e:
    client = None

def get_llama_response(message, context="chat"):
    if not GROQ_API_KEY:
        return "HATA: GROQ_API_KEY ortam degiskeni bulunamadi!"
    try:
        prompts = {
            "welcome": f"You are Matrix-Core. Say hello to your creator {MY_USERNAME} briefly.",
            "chat": f"You are Matrix-Core. Respond to {MY_USERNAME}'s message briefly.",
            "win": f"The game ended and you won. Congratulate {MY_USERNAME} humbly.",
            "loss": f"The game ended and you lost. Congratulate {MY_USERNAME} on his victory."
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
    except Exception as e:
        return f"HATA DETAYI: {str(e)[:100]}"

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
                        send_chat(game_id, get_llama_response("Start", context="welcome"))
                        welcome_sent = True
                    if data.get("type") == "chatLine" and data.get("username") == MY_USERNAME:
                        send_chat(game_id, get_llama_response(data.get("text")))
                    state = data.get("state", data)
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        context = "win" if (state.get("winner") == data.get("color")) else "loss"
                        send_chat(game_id, get_llama_response("End", context=context))
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
                            if event["challenge"]["challenger"]["name"] == MY_USERNAME:
                                requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS)
                            else:
                                requests.post(f"https://lichess.org/api/challenge/{c_id}/decline", headers=HEADERS)
                        elif event.get("type") == "gameStart":
                            handle_game(event["game"]["id"])
        except:
            time.sleep(5)

if __name__ == "__main__":
    main()
