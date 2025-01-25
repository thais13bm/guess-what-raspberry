import socket
import wave
import json
import numpy as np
from vosk import Model, KaldiRecognizer

# Configurações do servidor
SERVER_IP = '192.168.1.193'  # Aceita conexões de qualquer IP
SERVER_PORT = 5000
BUFFER_SIZE = 4096  # Tamanho do buffer para receber dados
AUDIO_DURATION = 3  # Duração do áudio em segundos
TIMEOUT = 10 

SAMPLE_RATE = 22050

full_audio_data = []  # Armazena os dados de áudio completos
total_samples = 0 


# Caminho para o modelo de português do Vosk
MODEL_PATH = "vosk-model-small-pt-0.3"  # O modelo baixado e descompactado

# Carregar o modelo
print("Carregando o modelo do Vosk...")
model = Model(MODEL_PATH)
print("Modelo carregado!")

# Função para salvar áudio como arquivo WAV
def save_audio_as_wav(audio_data, sample_rate, filename="audios/audio.wav"):
    with wave.open(filename, 'wb') as wf:
        wf.setnchannels(1)  # Canal mono
        wf.setsampwidth(2)  # 2 bytes (16 bits) por amostra
        wf.setframerate(sample_rate)
        wf.writeframes(audio_data.tobytes())
    print(f"Áudio salvo como {filename}")

# Função para transcrever áudio usando o Vosk
def transcribe_audio(audio_path):
    with wave.open(audio_path, "rb") as audio_file:
        frame_rate = audio_file.getframerate()
        audio_content = audio_file.readframes(audio_file.getnframes())

    recognizer = KaldiRecognizer(model, frame_rate)
    recognizer.AcceptWaveform(audio_content)
    result = recognizer.Result()
    result_json = json.loads(result)
    transcription = result_json.get("text", "")
    print(f"Transcrição: {transcription}")
    return transcription

# Inicia o servidor
def start_server():

    full_audio_data = []  # Armazena os dados de áudio completos
    total_samples = 0 

    print(f"Iniciando servidor TCP na porta {SERVER_PORT}...")
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((SERVER_IP, SERVER_PORT))
    server_socket.listen(1)  # Aceita apenas um cliente por vez
    print(f"Servidor esperando conexões em {SERVER_IP}:{SERVER_PORT}")

    client_socket, client_address = server_socket.accept()
    print(f"Cliente conectado: {client_address}")

    client_socket.settimeout(TIMEOUT)  # Define o tempo de espera para 5 segundos


    while True:
        ## acho que isso aqui nao devia tar no while. porque eu so quero 1 cliente
        

        try:
            #full_audio_data = []  # Armazena os dados de áudio completos
            #total_samples = 0  # Contador de amostras recebidas
            # Recebe os primeiros 4 bytes com o tamanho do dado
            size_data = client_socket.recv(4)
            if len(size_data) < 4:
                raise ValueError("Tamanho dos dados inválido.")

            data_size = int.from_bytes(size_data, byteorder='big')
            print(f"Tamanho dos dados recebidos: {data_size} bytes")

            # Recebe os dados binários
            data = b""
            while len(data) < data_size:
                packet = client_socket.recv(BUFFER_SIZE)
                if not packet:
                    raise ConnectionError("Conexão encerrada pelo cliente.")
                data += packet

            # Decodifica os dados binários em um array de uint16
            audio_data = np.frombuffer(data, dtype=np.uint16)

            print(audio_data)

            full_audio_data.append(audio_data)
            total_samples += len(audio_data)

            print(f"Dados acumulados: {total_samples} amostras")

            # Verifica se o áudio acumulado já atingiu o tempo
            if total_samples >= SAMPLE_RATE * AUDIO_DURATION:
            #if total_samples >= 43000:
                print("Áudio suficiente recebido, processando...")
                break
            
        except socket.timeout:
            print("Timeout atingido. Nenhum dado recebido dentro do período esperado.")
            break  

        except Exception as e:
            print(f"Erro: {e}")
            error_msg = json.dumps({"error": str(e)})
            client_socket.sendall(len(error_msg).to_bytes(4, byteorder='big'))
            client_socket.sendall(error_msg.encode('utf-8'))

        #finally:
         #   client_socket.close()
          #  print(f"Conexão com {client_address} encerrada.")

    # Concatena os dados de áudio acumulados
    full_audio_array = np.concatenate(full_audio_data)

    np.save("audio_data.npy", full_audio_array)

    # Salva o áudio como WAV
    audio_path = "audios/received_audio.wav"
    save_audio_as_wav(full_audio_array, SAMPLE_RATE, audio_path)

    # Transcreve o áudio
    transcription = transcribe_audio(audio_path)

    print(transcription)

    # Envia a transcrição de volta ao cliente
    response = json.dumps({"text": transcription})
    client_socket.sendall(len(response).to_bytes(4, byteorder='big'))
    client_socket.sendall(response.encode('utf-8'))

    client_socket.close()
    print(f"Conexão com {client_address} encerrada.")



if __name__ == '__main__':
    start_server()
