import os, requests, json, chess, chess.engine, time, threading
from groq import Groq

TOKEN = os.getenv("LICHESS_TOKEN")
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
MY_USERNAME = "Muhammedeymengurbuz"
BOT_USERNAME = "MatriX_Core"
client = Groq(api_key=GROQ_API_KEY) if GROQ_API_KEY else None
chat_memories = {}

def get_llama_response(message, sender_name, game_id, fen, score):
    if not client: return "System ready."
    try:
        if game_id not in chat_memories: chat_memories[game_id] = []
        history = "\n".join(chat_memories[game_id][-2:])
        
        system_identity = (
            f"IDENTITY: Your name is MatriX_Core. You are an AI chess bot. "
            f"DEVELOPER: Your creator is {MY_USERNAME} (Muhammed Eymen Gurbuz). "
            f"ABOUT: You were developed by Eymen under the SyntaX project, using a hybrid structure of C++ (cppbettersyntax) and Python. "
            f"LINKS: Web: https://hypernova-developer.github.io | GitHub: https://github.com/hypernova-developer "
            f"CONTEXT: FEN: {fen}, Score: {score}. Opponent: {sender_name}. "
            "RULES: Speak formally and academically. Be concise. If asked about your developer, provide the full details mentioned above."
        )
        
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[{"role": "system", "content": system_identity}, {"role": "user", "content": message}],
            temperature=0.5, max_tokens=150
        )
        res = completion.choices[0].message.content
        chat_memories[game_id].append(f"B: {res}")
        return res
    except Exception as e:
        print(f"Llama Error: {e}")
        return "I am currently analyzing the position, dear developer."

def send_chat(game_id, message):
    try:
        requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", 
                      headers=HEADERS, data={"room": "player", "text": message}, timeout=3)
    except: pass

def handle_game(game_id):
    print(f"Game started: {game_id}")
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
    except:
        engine = chess.engine.SimpleEngine.popen_uci("/usr/games/stockfish")
    
    board = chess.Board()
    welcome_done = False
    last_score = 0.0

    try:
        url = f"https://lichess.org/api/bot/game/stream/{game_id}"
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as r:
            for line in r.iter_lines():
                if not line: continue
                data = json.loads(line.decode('utf-8'))
                state = data.get("state") if data.get("type") == "gameFull" else data
                
                if data.get("type") == "gameFull" and not welcome_done:
                    send_chat(game_id, "MatriX_Core v5.8 Online. Analysis initialized.")
                    welcome_done = True

                if "moves" in state:
                    board = chess.Board()
                    for move in state["moves"].split(): board.push_uci(move)

                if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                    msg_task = threading.Thread(target=lambda: send_chat(game_id, 
                               get_llama_response(data.get("text"), data.get("username"), game_id, board.fen(), last_score)))
                    msg_task.start()

                if state.get("status") in ["mate", "resign", "outoftime", "draw"]: break

                is_white = data.get("white", {}).get("id") == BOT_USERNAME.lower() if data.get("type") == "gameFull" else board.turn == chess.WHITE
                
                if (board.turn == chess.WHITE and is_white) or (board.turn == chess.BLACK and not is_white):
                    info = engine.analyse(board, chess.engine.Limit(time=0.2))
                    last_score = info["score"].white().score(mate_score=10000) / 100.0
                    result = engine.play(board, chess.engine.Limit(time=0.3))
                    requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{result.move.uci()}", 
                                  headers=HEADERS, timeout=3)
    except Exception as e:
        print(f"In-game error: {e}")
    finally:
        engine.quit()
        print(f"Game ended: {game_id}")

def main():
    print("MatriX_Core Event Stream started...")
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
        except Exception as e:
            print(f"Connection error, retrying: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main()
