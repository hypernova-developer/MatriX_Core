import os
import requests
import json
import chess
import chess.engine
from groq import Groq

TOKEN = os.getenv("LICHESS_TOKEN")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
MY_USERNAME = "BURAYA_KENDI_KULLANICI_ADINI_YAZ"

client = Groq(api_key=GROQ_API_KEY)

def get_llama_response(message, context="chat"):
    try:
        prompts = {
            "welcome": f"You are Matrix-Core. Say hello to your creator {MY_USERNAME} and wish him luck.",
            "chat": f"You are Matrix-Core. Respond to {MY_USERNAME}'s message briefly and professionally.",
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
    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
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
                    continue

def main():
    event_url = "https://lichess.org/api/stream/event"
    with requests.get(event_url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        challenge_id = event["challenge"]["id"]
                        if event["challenge"]["challenger"]["name"] == MY_USERNAME:
                            requests.post(f"https://lichess.org/api/challenge/{challenge_id}/accept", headers=HEADERS)
                        else:
                            requests.post(f"https://lichess.org/api/challenge/{challenge_id}/decline", headers=HEADERS)
                    elif event.get("type") == "gameStart":
                        handle_game(event["game"]["id"])
                except:
                    continue

if __name__ == "__main__":
    main()
