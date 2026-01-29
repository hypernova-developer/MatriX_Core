import os
import requests
import json
import chess
import chess.engine

TOKEN = os.getenv("LICHESS_TOKEN")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}
BOT_USERNAME = "muhammedeymengurbuz" # Senin kullanıcı adın

def get_best_move(moves_str):
    try:
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        if moves_str:
            for move in moves_str.split():
                board.push_uci(move)
        
        # 1 saniye düşün ve hamle üret
        result = engine.play(board, chess.engine.Limit(time=1.0))
        engine.quit()
        return result.move.uci()
    except Exception as e:
        print(f"[ENGINE ERROR] {e}")
        return None

def handle_game(game_id):
    print(f"[GAME] Infiltrating Real Game ID: {game_id}")
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    
    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    data = json.loads(line.decode('utf-8'))
                    # Lichess bazen boş satırlar veya chat mesajları gönderir, kontrol edelim
                    if "state" in data or "moves" in data:
                        state = data.get("state", data)
                        moves = state.get("moves", "")
                        
                        best_move = get_best_move(moves)
                        if best_move:
                            print(f"[MOVE] Calculated for {game_id}: {best_move}")
                            res = requests.post(f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}", headers=HEADERS)
                            print(f"[LICHESS RESPONSE] {res.status_code}")
                except Exception as e:
                    continue

def main():
    print("Matrix-Core v2.2 (Python) - Sniper Precision Online")
    url = "https://lichess.org/api/stream/event"
    
    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                try:
                    event = json.loads(line.decode('utf-8'))
                    
                    if event.get("type") == "challenge":
                        c_id = event["challenge"]["id"]
                        requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS)
                        print(f"[EVENT] Accepted challenge: {c_id}")
                    
                    elif event.get("type") == "gameStart":
                        # İŞTE BURASI KRİTİK: 'gameId' yerine direkt 'game' objesinin içindeki 'id'yi alıyoruz
                        g_id = event["game"]["gameId"] if "gameId" in event["game"] else event["game"]["id"]
                        handle_game(g_id)
                except:
                    continue

if __name__ == "__main__":
    main()
