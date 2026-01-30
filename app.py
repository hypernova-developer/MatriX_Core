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
            f"FEN: {fen} | Skor: {score}. Resmi (Siz) ve akademik konuş. "
            "Web: hypernova-developer.github.io | GitHub: https://github.com/hypernova-developer"
        )
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[{"role": "system", "content": system_identity}, {"role": "user", "content": message}],
            temperature=0.6, max_tokens=100
        )
        res = completion.choices[0].message.content
        chat_memories[game_id].extend([f"U: {message}", f"B: {res}"])
        return res
    except: return "Analiz yapılıyor."

def send_chat(game_id, message):
    try: requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", headers=HEADERS, data={"room": "player", "text": message}, timeout=3)
    except: pass

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    try: engine = chess.engine.SimpleEngine.popen_uci("stockfish")
    except: engine = chess.engine.SimpleEngine.popen_uci("/usr/games/stockfish")
    
    board = chess.Board()
    welcome_sent = False
    last_score = 0.0

    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line or time.time() - START_TIME > 21000: break
                data = json.loads(line.decode('utf-8'))
                state = data.get("state", data)
                
                if "moves" in state:
                    board = chess.Board()
                    for move in state["moves"].split(): board.push_uci(move)

                if data.get("type") == "gameFull" and not welcome_sent:
                    send_chat(game_id, "MatriX_Core v5.6 Aktif. Sayın Geliştiricim, hoş geldiniz.")
                    welcome_sent = True

                if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                    response = get_llama_response(data.get("text"), data.get("username"), game_id, board.fen(), last_score)
                    send_chat(game_id, response)

                if state.get("status") in ["mate", "resign", "outoftime", "draw"]: break

                white_id = data.get("white", {}).get("id", "") if data.get("white") else ""
                black_id = data.get("black", {}).get("id", "") if data.get("black") else ""
                is_my_turn = (board.turn == chess.WHITE and white_id.lower() == BOT_USERNAME.lower()) or \
                             (board.turn == chess.BLACK and black_id.lower() == BOT_USERNAME.lower())

                if is_my_turn:
                    info = engine.analyse(board, chess.engine.Limit(time=0.3))
                    last_score = info["score"].white().score(mate_score=10000) / 100.0
                    result = engine.play(board, chess.engine.Limit(time=0.4))
                    requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{result.move.uci()}", headers=HEADERS, timeout=5)
    except: pass
    finally: engine.quit()

def main():
    while True:
        if time.time() - START_TIME > 21000: break
        try:
            with requests.get("https://lichess.org/api/stream/event", headers=HEADERS, stream=True, timeout=60) as r:
                for line in r.iter_lines():
                    if not line or time.time() - START_TIME > 21000: break
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        action = "accept" if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower() else "decline"
                        requests.post(f"https://lichess.org/api/challenge/{c_id}/{action}", headers=HEADERS, timeout=5)
                    elif event.get("type") == "gameStart":
                        handle_game(event["game"]["id"])
        except: time.sleep(2)

if __name__ == "__main__":
    main()
