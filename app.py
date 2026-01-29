import os
import requests
import json
import subprocess
import chess
import chess.engine

# Token and Headers
TOKEN = os.getenv("LICHESS_TOKEN")
HEADERS = {"Authorization": f"Bearer {TOKEN}"}

def get_best_move(moves_str):
    """Stockfish kullanarak en iyi hamleyi bulur."""
    try:
        # Stockfish'i başlat
        engine = chess.engine.SimpleEngine.popen_uci("stockfish")
        board = chess.Board()
        
        # Hamle geçmişini uygula
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
    """Oyun akışını (stream) takip eder."""
    print(f"[GAME] Infiltrating game: {game_id}")
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    
    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                data = json.loads(line.decode('utf-8'))
                
                # Sadece hamle veya oyun başlangıcı durumunda işlem yap
                if data.get("type") in ["gameFull", "gameState"]:
                    moves = data.get("state", {}).get("moves", "") if data.get("type") == "gameFull" else data.get("moves", "")
                    
                    # Sıra botta mı kontrol et (Basit mantık: Beyazsa hamle sayısı çift, siyahsa tek olmalı)
                    move_list = moves.split() if moves else []
                    # Botun rengini öğrenelim (gameFull içinden)
                    if data.get("type") == "gameFull":
                        bot_is_white = data["white"].get("id") == "muhammedeymengurbuz" # Burayı kontrol et
                    
                    # Hamle üret ve gönder
                    best_move = get_best_move(moves)
                    if best_move:
                        print(f"[MOVE] Calculated best move: {best_move}")
                        post_url = f"https://lichess.org/api/bot/game/{game_id}/move/{best_move}"
                        res = requests.post(post_url, headers=HEADERS)
                        if res.status_code == 200:
                            print(f"[SUCCESS] Move {best_move} sent.")
                        else:
                            print(f"[FAILED] Move error: {res.text}")

def main():
    print("Matrix-Core v2.1 (Python Edition) Online.")
    url = "https://lichess.org/api/stream/event"
    
    with requests.get(url, headers=HEADERS, stream=True) as response:
        for line in response.iter_lines():
            if line:
                event = json.loads(line.decode('utf-8'))
                
                # Meydan okumaları kabul et
                if event.get("type") == "challenge":
                    c_id = event["challenge"]["id"]
                    requests.post(f"https://lichess.org/api/challenge/{c_id}/accept", headers=HEADERS)
                    print(f"[EVENT] Accepted challenge: {c_id}")
                
                # Oyun başladığında içeri sız
                elif event.get("type") == "gameStart":
                    g_id = event["game"]["id"]
                    handle_game(g_id)

if __name__ == "__main__":
    main()
