import os, requests, json, chess, chess.engine, time
from groq import Groq

TOKEN = os.getenv("LICHESS_TOKEN")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
MY_USERNAME = "Muhammedeymengurbuz"
BOT_USERNAME = "MatriX_Core"
client = Groq(api_key=GROQ_API_KEY) if GROQ_API_KEY else None
chat_memories = {}
START_TIME = time.time()

def get_llama_response(message, sender_name, game_id, fen, score):
    if not client: return "Sistem hazır."
    try:
        if game_id not in chat_memories: chat_memories[game_id] = []
        history = "\n".join(chat_memories[game_id][-2:])
        system_identity = (
            f"Adın MatriX_Core. Geliştiricin: {MY_USERNAME}. Muhatabın: {sender_name}. "
            f"FEN: {fen} | Skor: {score}. Resmi ve kısa konuş. "
            "Gerekirse: hypernova-developer.github.io"
        )
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[{"role": "system", "content": system_identity}, {"role": "user", "content": message}],
            temperature=0.6, max_tokens=60
        )
        res = completion.choices[0].message.content
        chat_memories[game_id].append(f"U: {message}")
        chat_memories[game_id].append(f"B: {res}")
        return res
    except: return "Analiz ediliyor."

def send_chat(game_id, message):
    try: requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", headers=HEADERS, data={"room": "player", "text": message}, timeout=2)
    except: pass

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    try: engine = chess.engine.SimpleEngine.popen_uci("stockfish")
    except: engine = chess.engine.SimpleEngine.popen_uci("/usr/games/stockfish")
    
    board = chess.Board()
    welcome_done = False

    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line: continue
                data = json.loads(line.decode('utf-8'))
                
                state = data.get("state") if data.get("type") == "gameFull" else data
                if data.get("type") == "gameFull":
                    state = data.get("state")
                    if not welcome_done:
                        send_chat(game_id, "MatriX_Core v5.7 bağlandı. Sayın Geliştiricim, hazırım.")
                        welcome_done = True
                
                if "moves" in state:
                    new_board = chess.Board()
                    for move in state["moves"].split(): new_board.push_uci(move)
                    board = new_board

                if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                    resp = get_llama_response(data.get("text"), data.get("username"), game_id, board.fen(), 0.0)
                    send_chat(game_id, resp)

                if state.get("status") in ["mate", "resign", "outoftime", "draw"]: break

                white_id = data.get("white", {}).get("id", "") if data.get("type") == "gameFull" else ""
                
                if (board.turn == chess.WHITE and (white_id.lower() == BOT_USERNAME.lower() or not white_id)) or \
                   (board.turn == chess.BLACK and (not white_id or white_id.lower() != BOT_USERNAME.lower())):
                    result = engine.play(board, chess.engine.Limit(time=0.2)) 
                    requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{result.move.uci()}", headers=HEADERS, timeout=3)
    except: pass
    finally: engine.quit()

def main():
    while True:
        try:
            with requests.get("https://lichess.org/api/stream/event", headers=HEADERS, stream=True, timeout=60) as r:
                for line in r.iter_lines():
                    if not line: continue
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower():
                            requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS)
                    elif event.get("type") == "gameStart":
                        handle_game(event["game"]["id"])
        except: time.sleep(2)

if __name__ == "__main__":
    main()
