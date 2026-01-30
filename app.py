import os
import requests
import json
import chess
import chess.engine
import time
import threading
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

def get_llama_response(message, sender_name, game_id, fen, score):
    if not client: return "Sistem aktif."
    try:
        if game_id not in chat_memories: chat_memories[game_id] = []
        history = "\n".join(chat_memories[game_id][-3:])
        system_identity = (
            f"Adın MatriX_Core. Geliştiricin: {MY_USERNAME}. Muhatabın: {sender_name}. "
            f"Tahta: {fen} | Skor: {score}. Resmi (Siz) ve akademik konuş. "
            f"Web: hypernova-developer.github.io | GitHub: https://github.com/hypernova-developer"
        )
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[{"role": "system", "content": system_identity}, {"role": "user", "content": message}],
            temperature=0.6,
            max_tokens=150
        )
        res = completion.choices[0].message.content
        chat_memories[game_id].append(f"U: {message}")
        chat_memories[game_id].append(f"B: {res}")
        return res
    except:
        return "Analiz süreci devam ediyor."

def send_chat(game_id, message):
    requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", headers=HEADERS, data={"room": "player", "text": message}, timeout=5)

def get_engine_data(moves_str):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split(): board.push_uci(move)
        info = engine.analyse(board, chess.engine.Limit(time=0.1))
        score = info["score"].white().score(mate_score=10000) / 100.0
        bm = info["pv"][0].uci() if "pv" in info else None
        fen = board.fen()
        engine.quit()
        return bm, fen, score
    except:
        return None, "", 0.0

def process_chat(game_id, sender, text, moves):
    bm, f, s = get_engine_data(moves)
    response = get_llama_response(text, sender, game_id, f, s)
    send_chat(game_id, response)

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False
    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line: continue
                data = json.loads(line.decode('utf-8'))
                state = data.get("state", data)
                moves = state.get("moves", "")
                
                if data.get("type") == "gameFull" and not welcome_sent:
                    send_chat(game_id, "MatriX_Core v5.2 Threading-Ready. Hoş geldiniz Sayın Geliştiricim.")
                    welcome_sent = True
                
                if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                    t = threading.Thread(target=process_chat, args=(game_id, data.get("username"), data.get("text"), moves))
                    t.start()
                
                if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                    if game_id in chat_memories: del chat_memories[game_id]
                    break
                
                if "moves" in state or data.get("type") == "gameFull":
                    bm, _, _ = get_engine_data(moves)
                    if bm: requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{bm}", headers=HEADERS, timeout=5)
    except:
        pass

def main():
    while True:
        try:
            with requests.get("https://lichess.org/api/stream/event", headers=HEADERS, stream=True, timeout=60) as r:
                for line in r.iter_lines():
                    if not line: continue
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        action = "accept" if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower() else "decline"
                        requests.post(f"https://lichess.org/api/challenge/{c_id}/{action}", headers=HEADERS)
                    elif event.get("type") == "gameStart":
                        threading.Thread(target=handle_game, args=(event["game"]["id"],)).start()
        except:
            time.sleep(2)

if __name__ == "__main__":
    main()
