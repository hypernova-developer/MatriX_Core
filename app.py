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

def get_llama_response(message, sender_name, game_id, fen, score):
    if not client: return "Sistem çevrimiçi."
    try:
        if game_id not in chat_memories:
            chat_memories[game_id] = []
        
        history_context = "\n".join(chat_memories[game_id][-3:])
        status = f"Tahta Durumu (FEN): {fen} | Oyun Puanı: {score}"
        
        system_identity = (
            f"Adın MatriX_Core. Geliştiricin: {MY_USERNAME}. "
            f"Muhatabın: {sender_name}. {status}. "
            f"Geçmiş:\n{history_context}\n"
            "KURALLAR: "
            "1. RESMİ DİL: Her zaman 'Siz' diye hitap edin, vakur ve akademik olun. "
            f"2. Geliştiricinize 'Sayın Geliştiricim', başkalarına 'Sayın Rakibim' deyin. "
            "3. OYUN ANALİZİ: Oyun puanı pozitifse (+2 vb.) üstün olduğunuzu, negatifse (-2 vb.) rakibin üstün olduğunu bilerek yorum yapın. "
            "4. LİNKLER (ZORUNLU): Geliştirici/Web sitesi sorulursa: 'GitHub: https://github.com/hypernova-developer | Web: hypernova-developer.github.io' linklerini verin. "
            "5. 'Seni tanıyalım' denirse: 'hypernova-developer.github.io/MatriX' adresini verin."
        )
        
        completion = client.chat.completions.create(
            model="llama-3.3-70b-versatile",
            messages=[
                {"role": "system", "content": system_identity},
                {"role": "user", "content": message}
            ],
            temperature=0.6,
            max_tokens=150
        )
        
        response = completion.choices[0].message.content
        chat_memories[game_id].append(f"User: {message}")
        chat_memories[game_id].append(f"Bot: {response}")
        return response
    except:
        return "Sistem analiz gerçekleştiriyor."

def get_engine_analysis(moves_str):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split():
                board.push_uci(move)
        
        info = engine.analyse(board, chess.engine.Limit(time=0.5))
        score = info["score"].white().score(mate_score=10000) / 100.0
        best_move = info["pv"][0].uci() if "pv" in info else None
        fen = board.fen()
        engine.quit()
        return best_move, fen, score
    except:
        return None, None, 0.0

def handle_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    welcome_sent = False
    last_fen = ""
    last_score = 0.0
    try:
        with requests.get(url, headers=HEADERS, stream=True, timeout=60) as response:
            for line in response.iter_lines():
                if not line: continue
                try:
                    data = json.loads(line.decode('utf-8'))
                    state = data.get("state", data)
                    moves = state.get("moves", "")
                    
                    best_move, current_fen, current_score = get_engine_analysis(moves)
                    last_fen, last_score = current_fen, current_score

                    if data.get("type") == "gameFull" and not welcome_sent:
                        send_chat(game_id, "MatriX_Core v5.0 bağlandı. Sayın Geliştiricim, analiz başlatıldı.")
                        welcome_sent = True
                    
                    if data.get("type") == "chatLine" and data.get("username").lower() != BOT_USERNAME.lower():
                        msg = data.get("text")
                        send_chat(game_id, get_llama_response(msg, data.get("username"), game_id, last_fen, last_score))
                    
                    if state.get("status") in ["mate", "resign", "outoftime", "draw"]:
                        send_chat(game_id, "Müsabaka tamamlanmıştır. Verimli bir analiz süreciydi.")
                        if game_id in chat_memories: del chat_memories[game_id]
                        break
                    
                    if best_move and ("moves" in state or data.get("type") == "gameFull"):
                        requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}", headers=HEADERS, timeout=5)
                except:
                    continue
    except:
        pass

def send_chat(game_id, message):
    try:
        requests.post(f"https://lichess.org/api/bot/game/{game_id}/chat", headers=HEADERS, data={"room": "player", "text": message}, timeout=5)
    except:
        pass

def main():
    while True:
        try:
            with requests.get("https://lichess.org/api/stream/event", headers=HEADERS, stream=True, timeout=60) as response:
                for line in response.iter_lines():
                    if not line: continue
                    event = json.loads(line.decode('utf-8'))
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        action = "accept" if event["challenge"]["challenger"]["name"].lower() == MY_USERNAME.lower() else "decline"
                        requests.post(f"https://lichess.org/api/challenge/{c_id}/{action}", headers=HEADERS, timeout=5)
                    elif event.get("type") == "gameStart":
                        handle_game(event["game"]["id"])
        except:
            time.sleep(2)

if __name__ == "__main__":
    main()
