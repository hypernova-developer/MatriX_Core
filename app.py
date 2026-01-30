import os, requests, json, chess, chess.engine, time, threading
from groq import Groq

TOKEN = os.getenv("LICHESS_TOKEN")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
MY_USERNAME = "Muhammedeymengurbuz"
BOT_USERNAME = "MatriX_Core"
client = Groq(api_key=GROQ_API_KEY) if GROQ_API_KEY else None
chat_memories = {}

def get_llama_response(message, sender_name, game_id):
    if not client: return "System online."
    try:
        if game_id not in chat_memories: chat_memories[game_id] = []
        
        system_identity = (
            f"Your name: MatriX_Core. Creator: {MY_USERNAME}. Project: SyntaX. "
            f"If asked about developer, explicitly name {MY_USERNAME}. "
            f"Opponent: {sender_name}. Speak formal Turkish (Siz/Biz). Be very brief."
        )
        
        messages = [{"role": "system", "content": system_identity}]
        for mem in chat_memories[game_id][-2:]:
            role = "assistant" if mem.startswith("B:") else "user"
            messages.append({"role": role, "content": mem.replace("B: ", "").replace("U: ", "")})
        messages.append({"role": "user", "content": message})

        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=messages,
            temperature=0.3,
            max_tokens=100
        )
        res = completion.choices[0].message.content
        chat_memories[game_id].append(f"U: {message}")
        chat_memories[game_id].append(f"B: {res}")
        return res
    except:
        return "Analysis in progress, sir."

def send_chat(game_id, message):
    try:
        requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", 
                      headers=HEADERS, data={"room": "player", "text": message}, timeout=2)
    except: pass

def handle_game(game_id):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
    except:
        engine = chess.engine.SimpleEngine.popen_uci("/usr/games/stockfish")
    
    board = chess.Board()
    welcome_done = False

    try:
        url = f"https://lichess.org/api/bot/game/stream/{game_id}"
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line: continue
                data = json.loads(line.decode('utf-8'))
                state = data.get("state") if data.get("type") == "gameFull" else data
                
                if data.get("type") == "gameFull" and not welcome_done:
                    send_chat(game_id, "MatriX_Core v6.0 Ready. How can I help you, developer?")
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
                    result = engine.play(board, chess.engine.Limit(time=0.1))
                    requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{result.move.uci()}", headers=HEADERS, timeout=2)
    except: pass
    finally:
        engine.quit()

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
                            requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS, timeout=5)
                    elif event.get("type") == "gameStart":
                        threading.Thread(target=handle_game, args=(event["game"]["id"],)).start()
        except:
            time.sleep(5)

if __name__ == "__main__":
    main()
