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

chat_memories = {}

def get_llama_response(message, sender_name, game_id):
    if not client: return "System online."
    try:
        if game_id not in chat_memories:
            chat_memories[game_id] = []
        
        history_context = "\n".join(chat_memories[game_id][-5:])
        
        system_identity = (
            f"Your name is MatriX_Core. Your developer is {MY_USERNAME}. "
            f"Current user: {sender_name}. "
            f"History:\n{history_context}\n"
            "STRICT RULES: "
            f"1. If user is NOT '{MY_USERNAME}', call them 'Dostum' or its translation. "
            f"2. If asked about developer: 'Geliştiricim {MY_USERNAME}. GitHub: https://github.com/hypernova-developer | Web: hypernova-developer.github.io'. "
            "3. If asked to 'know you better': 'hypernova-developer.github.io/MatriX linkinden benimle ilgili bilgilere ulaşabilirsiniz.' "
            "4. Always respond in the user's language and be concise."
        )
        
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[
                {"role": "system", "content": system_identity},
                {"role": "user", "content": message}
            ],
            temperature=0.8,
            max_tokens=150
        )
        
        response = completion.choices[0].message.content
        chat_memories[game_id].append(f"User: {message}")
        chat_memories[game_id].append(f"Bot: {response}")
        return response
    except:
        return "System online."

def send_chat(game_id, message):
    try:
        url = f"https://lichess.org/api/bot/game/{game_id}/chat"
        requests.post(url, headers=HEADERS, data={"room": "player", "text": message}, timeout=5)
    except:
        pass

def get_best_move(moves_str):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split():
                board.push_uci(move)
        result = engine.play(board, chess.engine.Limit(time=0.8))
        engine.quit()
        return result.move.uci()
    except:
        return None

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False
    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as response:
            for line in response.iter_lines():
                if not line: continue
                try:
                    data = json.loads(line.decode('utf-8'))
                    if data.get("type") == "gameFull" and not welcome_sent:
                        send_chat(game_id, "MatriX_Core system online. Let's play!")
                        welcome_sent = True
                    if data.get("type") == "chatLine":
                        sender = data.get("username")
                        if sender.lower() != BOT_USERNAME.lower():
                            msg = data.get("text")
                            send_chat(game_id, get_llama_response(msg, sender, game_id))
                    state = data.get("state", data)
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        send_chat(game_id, "Good game! See you next time.")
                        if game_id in chat_memories: del chat_memories[game_id]
                        break
                    if "moves" in state or data.get("type") == "gameFull":
                        best_move = get_best_move(state.get("moves", ""))
                        if best_move:
                            requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}", headers=HEADERS, timeout=5)
                except:
                    continue
    except:
        pass

def main():
    event_url = "https://lichess.org/api/stream/event"
    while True:
        try:
            with requests.get(event_url, headers=HEADERS, stream=True, timeout=60) as response:
                for line in response.iter_lines():
                    if not line: continue
                    try:
                        event = json.loads(line.decode('utf-8'))
                        if event.get("type") == "challenge":
                            c_id = event["challenge"]["id"]
                            action = "accept" if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower() else "decline"
                            requests.post(f"https://lichess.org/api/challenge/{c_id}/{action}", headers=HEADERS, timeout=5)
                        elif event.get("type") == "gameStart":
                            handle_game(event["game"]["id"])
                    except:
                        continue
        except:
            time.sleep(2)

if __name__ == "__main__":
    main()
