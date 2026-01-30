import os, requests, json, chess, chess.engine, time, threading
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
        history = "\n".join(chat_memories[game_id][-3:])
        system_identity = (
            f"Adın MatriX_Core. Geliştiricin: {MY_USERNAME}. Muhatabın: {sender_name}. "
            f"Tahta: {fen} | Skor: {score}. Resmi (Siz) ve akademik konuş. "
            "Web: hypernova-developer.github.io | GitHub: https://github.com/hypernova-developer"
        )
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[{"role": "system", "content": system_identity}, {"role": "user", "content": message}],
            temperature=0.6,
            max_tokens=200
        )
        res = completion.choices[0].message.content
        chat_memories[game_id].extend([f"U: {message}", f"B: {res}"])
        return res
    except: return "Analiz süreci devam ediyor."

def send_chat(game_id, message):
    try:
        requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", headers=HEADERS, data={"room": "player", "text": message}, timeout=5)
    except: pass

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
    except:
        engine = chess.engine.SimpleEngine.popen_uci("/usr/games/stockfish")
    
    board = chess.Board()
    welcome_sent = False
    
    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line: continue
                data = json.loads(line.decode('utf-8'))
                state = data.get("state", data)
                
                # 1. Hamle Takibi ve Board Güncelleme
                if "moves" in state:
                    board = chess.Board()
                    for move in state["moves"].split():
                        board.push_uci(move)

                # 2. Hoşgeldin Mesajı
                if data.get("type") == "gameFull" and not welcome_sent:
                    send_chat(game_id, "MatriX_Core v5.4 aktif. Sayın Geliştiricim, sistem analize hazır.")
                    welcome_sent = True

                # 3. Sohbet (Ayrı Thread - Hamleyi Engellemez)
                if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                    info = engine.analyse(board, chess.engine.Limit(time=0.1))
                    score = info["score"].white().score(mate_score=10000) / 100.0
                    threading.Thread(target=lambda: send_chat(game_id, get_llama_response(data.get("text"), data.get("username"), game_id, board.fen(), score))).start()

                # 4. Oyun Bitiş Kontrolü
                if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                    break

                # 5. Hamle Yapma (En Kritik Kısım)
                is_white = data.get("white", {}).get("id") == BOT_USERNAME.lower() if data.get("white") else False
                is_black = data.get("black", {}).get("id") == BOT_USERNAME.lower() if data.get("black") else False
                
                if (board.turn == chess.WHITE and is_white) or (board.turn == chess.BLACK and is_black):
                    result = engine.play(board, chess.engine.Limit(time=0.5))
                    requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{result.move.uci()}", headers=HEADERS, timeout=5)
    finally:
        engine.quit()

def main():
    while True:
        if time.time() - START_TIME > 20000: break
        try:
            with requests.get("https://lichess.org/api/stream/event", headers=HEADERS, stream=True, timeout=60) as r:
                for line in r.iter_lines():
                    if time.time() - START_TIME > 20000: break
                    if not line: continue
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        action = "accept" if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower() else "decline"
                        requests.post(f"https://lichess.org/api/challenge/{c_id}/{action}", headers=HEADERS, timeout=5)
                    elif event.get("type") == "gameStart":
                        threading.Thread(target=handle_game, args=(event["game"]["id"],)).start()
        except: time.sleep(2)

if __name__ == "__main__":
    main()
