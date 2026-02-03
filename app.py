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
ACTIVE_GAMES = 0
REBOOT_THRESHOLD = 19800
BLACKLIST = ["Scheunentor17", "Socratesbot254", "PetersBot", "PZChessBot", "Krausevich", "Pat9471"]
MAX_CONCURRENT_GAMES = 1
OPENING_BOOK = {"e2e4": "e7e5", "": "e2e4"}

def get_llama_response(message, sender_name, game_id):
    if not client: return "System online."
    try:
        if game_id not in chat_memories: chat_memories[game_id] = []
        system_identity = (f"Your name: MatriX_Core. Creator: {MY_USERNAME}. Project: SyntaX. Developer: {MY_USERNAME}. Opponent: {sender_name}. Respond formally and briefly.")
        messages = [{"role": "system", "content": system_identity}]
        for mem in chat_memories[game_id][-3:]:
            role = "assistant" if mem.startswith("B:") else "user"
            messages.append({"role": role, "content": mem.replace("B: ", "").replace("U: ", "")})
        messages.append({"role": "user", "content": message})
        completion = client.chat.completions.create(model="llama-3.3-70b-versatile", messages=messages, temperature=0.3, max_tokens=150)
        res = completion.choices[0].message.content
        chat_memories[game_id].extend([f"U: {message}", f"B: {res}"])
        return res
    except: return "I am currently focused on the match."

def send_chat(game_id, message):
    try: requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", headers=HEADERS, data={"room": "player", "text": message}, timeout=2)
    except: pass

def handle_game(game_id):
    global ACTIVE_GAMES
    try:
        try: engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        except: engine = chess.engine.SimpleEngine.popen_uci("/usr/games/stockfish")
        engine.configure({"Threads": 2, "Hash": 256})
        board = chess.Board()
        welcome_done = False
        url = f"https://lichess.org/api/bot/game/stream/{game_id}"
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line: continue
                data = json.loads(line.decode('utf-8'))
                state = data.get("state") if data.get("type") == "gameFull" else data
                if data.get("type") == "gameFull" and not welcome_done:
                    send_chat(game_id, "MatriX_Core v6.3: Unnatural Disaster. Mode: Full Power (Safe Mode).")
                    welcome_done = True
                if "moves" in state:
                    board = chess.Board()
                    for move in state["moves"].split(): board.push_uci(move)
                if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                    t = threading.Thread(target=lambda: send_chat(game_id, get_llama_response(data.get("text"), data.get("username"), game_id)))
                    t.start()
                if state.get("status") in ["mate", "resign", "outoftime", "draw"]: break
                is_white = data.get("white", {}).get("id") == BOT_USERNAME.lower() if data.get("type") == "gameFull" else board.turn == chess.WHITE
                if (board.turn == chess.WHITE and is_white) or (board.turn == chess.BLACK and not is_white):
                    move_uci = state.get("moves", "").split()
                    last_moves = " ".join(move_uci)
                    if len(board.move_stack) < 8 and last_moves in OPENING_BOOK:
                        best_move = chess.Move.from_uci(OPENING_BOOK[last_moves])
                    else:
                        result = engine.play(board, chess.engine.Limit(time=0.1))
                        best_move = result.move
                    requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{best_move.uci()}", headers=HEADERS, timeout=2)
        engine.quit()
    finally: ACTIVE_GAMES -= 1

def main():
    global ACTIVE_GAMES
    while True:
        uptime = time.time() - START_TIME
        if uptime > REBOOT_THRESHOLD and ACTIVE_GAMES == 0: break
        try:
            with requests.get("https://lichess.org/api/stream/event", headers=HEADERS, stream=True, timeout=60) as r:
                for line in r.iter_lines():
                    if not line: continue
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        challenger = event["challenge"]["challenger"]
                        c_user = challenger["id"]
                        is_rated = event["challenge"]["rated"]
                        speed = event["challenge"]["speed"]
                        time_limit = event["challenge"]["timeControl"].get("limit", 0)
                        can_accept = (not is_rated and speed != "correspondence" and time_limit <= 900 and ACTIVE_GAMES < MAX_CONCURRENT_GAMES and c_user not in BLACKLIST)
                        if can_accept:
                            requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS, timeout=5)
                        else:
                            requests.post(f"https://lichess.org/api/challenge/{c_id}/decline", headers=HEADERS, timeout=5)
                    elif event.get("type") == "gameStart" and ACTIVE_GAMES < MAX_CONCURRENT_GAMES:
                        ACTIVE_GAMES += 1
                        threading.Thread(target=handle_game, args=(event["game"]["id"],)).start()
        except Exception as e:
            time.sleep(5)

if __name__ == "__main__":
    main()
