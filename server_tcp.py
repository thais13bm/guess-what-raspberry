import socket
import wave
import json
import numpy as np
from vosk import Model, KaldiRecognizer
import noisereduce as nr
import sounddevice as sd
#from playsound import playsound
#import simpleaudio as sa
import winsound
import scipy
import scipy.io.wavfile as wavfile
import tensorflow as tf
import tensorflow_hub as hub
import numpy as np
import csv




# Configurações do servidor
SERVER_IP = '192.168.1.193'  # Aceita conexões de qualquer IP
SERVER_PORT = 5000
BUFFER_SIZE = 4096  # Tamanho do buffer para receber dados
AUDIO_DURATION = 2  # Duração do áudio em segundos
TIMEOUT = 10 

SAMPLE_RATE = 22050

full_audio_data = []  # Armazena os dados de áudio completos
total_samples = 0 


# Caminho para o modelo de português do Vosk
MODEL_PATH = "vosk-model-small-pt-0.3"  # O modelo baixado e descompactado

# Carregar o modelo
#print("Carregando o modelo do Vosk...")
#model = Model(MODEL_PATH)
#print("Modelo carregado!")



# Find the name of the class with the top score when mean-aggregated across frames.
def class_names_from_csv(class_map_csv_text):
  """Returns list of class names corresponding to score vector."""
  class_names = []
  with tf.io.gfile.GFile(class_map_csv_text) as csvfile:
    reader = csv.DictReader(csvfile)
    for row in reader:
      class_names.append(row['display_name'])

  return class_names



def ensure_sample_rate(original_sample_rate, waveform,
                       desired_sample_rate=16000):
  """Resample waveform if required."""
  if original_sample_rate != desired_sample_rate:
    desired_length = int(round(float(len(waveform)) /
                               original_sample_rate * desired_sample_rate))
    waveform = scipy.signal.resample(waveform, desired_length)
  return desired_sample_rate, waveform



def play_audio(audio_array, sample_rate):
    """Reproduz o áudio diretamente e aguarda até que termine."""
    audio_array_float = audio_array.astype(np.float32)
    audio_array_float /= np.max(np.abs(audio_array_float))  # Normalizando
    

    sd.play(audio_array_float, samplerate=sample_rate)
    sd.wait()  # Aguarda a reprodução terminar



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



def classify_audio(audio_path):
    wav_file_name = audio_path
    sample_rate, wav_data = wavfile.read(wav_file_name, 'rb')
    sample_rate, wav_data = ensure_sample_rate(sample_rate, wav_data)

    waveform = wav_data / tf.int16.max

    # Run the model, check the output.
    scores, embeddings, spectrogram = model(waveform)

    scores_np = scores.numpy()
    spectrogram_np = spectrogram.numpy()
    infered_class = class_names[scores_np.mean(axis=0).argmax()]
    print(f'The main sound is: {infered_class}')
    return infered_class



def process_audio(full_audio_data, sample_rate):
    # Concatena os dados de áudio acumulados
    full_audio_array = np.concatenate(full_audio_data)

    audio_no_noise = nr.reduce_noise(y = full_audio_array, sr=sample_rate)


    #audio_repeated = np.tile(audio_no_noise, 4)

    # Salva o áudio como arquivo WAV
    audio_path = "audios/received_audio.wav"
    save_audio_as_wav(audio_no_noise, sample_rate, audio_path)

    winsound.PlaySound(audio_path, winsound.SND_FILENAME)
    #play_audio(audio_no_noise, sample_rate)

    # Transcreve o áudio
    audio_class = classify_audio(audio_path)
    #print(f"Transcrição: {transcription}")

    return audio_class

model = hub.load('https://tfhub.dev/google/yamnet/1')

class_map_path = model.class_map_path().numpy()
class_names = class_names_from_csv(class_map_path)




def start_server():

    print(f"Iniciando servidor TCP na porta {SERVER_PORT}...")
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((SERVER_IP, SERVER_PORT))
    server_socket.listen(1)  # Aceita apenas um cliente por vez
    print(f"Servidor esperando conexões em {SERVER_IP}:{SERVER_PORT}")

    client_socket, client_address = server_socket.accept()
    print(f"Cliente conectado: {client_address}")

    client_socket.settimeout(TIMEOUT)  # Define o tempo de espera para 5 segundos

    return client_socket, client_address

# Inicia o servidor
def receive_data():
    full_audio_data = []  # Armazena os dados de áudio completos
    total_samples = 0

    client_socket, client_address = start_server()
    print(f"Conexão estabelecida com o cliente {client_address}.")

    while True:
        
        try:
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

            # Se o tamanho do dado for 1, interpretamos como a flag "LISTEN"
            if data_size == 2 and data[0] == 1:
                print("Flag LISTEN recebida, processando o áudio acumulado...")
                # Chama a função para processar o áudio
                transcription = process_audio(full_audio_data, SAMPLE_RATE)

                #client_socket.sendall(len(transcription).to_bytes(4, byteorder="big"))  ##talvez descomentar isso aq
                client_socket.sendall(transcription.encode("utf-8"))


                # Reseta o acumulador para próximo áudio 
                ##vamos pensar em talvez nao resetar ne, nao sei
                ### colocar mais um input aqui, perguntando se quer acumular ou quer resetar

                full_audio_data = []
                total_samples = 0
            
            else:
                # Caso contrário, trata como dados de áudio
                audio_data = np.frombuffer(data, dtype=np.uint16)
                print(f"Recebendo áudio: {audio_data}")
                full_audio_data.append(audio_data)
                total_samples += len(audio_data)

                print(f"Dados acumulados: {total_samples} amostras")

        except socket.timeout:
            print("Timeout atingido. Nenhum dado recebido dentro do período esperado.")
            opcao = int(input("Digite 1 para continuar e 2 para reiniciar o servidor: "))  ##manter isso para fins de debug e simplicidade. mas ao enviar, vamos remover

            if opcao == 2:
                #client_socket.close()
                #print(f"Conexão com {client_address} encerrada.")
                client_socket, client_address = start_server()   ##talvez o correto aqui fosse um break
                
        except Exception as e:
            print(f"Erro: {e}")
            #error_msg = json.dumps({"error": str(e)})
            #client_socket.sendall(len(error_msg).to_bytes(4, byteorder='big'))
            #client_socket.sendall(error_msg.encode('utf-8'))

    client_socket.close()
    print(f"Conexão com {client_address} encerrada.")


if __name__ == '__main__':
    receive_data()
