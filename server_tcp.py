import socket
import wave

import numpy as np
import noisereduce as nr
import sounddevice as sd

import scipy
import scipy.io.wavfile as wavfile
import tensorflow as tf
import tensorflow_hub as hub
import numpy as np
import csv




# Configurações do servidor
SERVER_IP = '0.0.0.0'  # Aceita conexões de qualquer IP
SERVER_PORT = 5000
BUFFER_SIZE = 132300  # Tamanho do buffer para receber dados
TIMEOUT = 30
SAMPLE_RATE = 22050

full_audio_data = []  # Armazena os dados de áudio completos
total_samples = 0 




# achar o nome da classe com maior score.
def class_names_from_csv(class_map_csv_text):
 
  class_names = []
  with tf.io.gfile.GFile(class_map_csv_text) as csvfile:
    reader = csv.DictReader(csvfile)
    for row in reader:
      class_names.append(row['display_name'])

  return class_names



def ensure_sample_rate(original_sample_rate, waveform,
                       desired_sample_rate=16000):
 
  if original_sample_rate != desired_sample_rate:
    desired_length = int(round(float(len(waveform)) /
                               original_sample_rate * desired_sample_rate))
    waveform = scipy.signal.resample(waveform, desired_length)
  return desired_sample_rate, waveform




# Função para salvar áudio como arquivo WAV
def save_audio_as_wav(audio_data, sample_rate, filename="audios/audio.wav"):
    with wave.open(filename, 'wb') as wf:
        wf.setnchannels(1)  # Canal mono
        wf.setsampwidth(2)  # 2 bytes (16 bits) por amostra
        wf.setframerate(sample_rate)
        wf.writeframes(audio_data.tobytes())
    print(f"Áudio salvo como {filename}")




def classify_audio(audio_path):
    wav_file_name = audio_path
    sample_rate, wav_data = wavfile.read(wav_file_name, 'rb')
    sample_rate, wav_data = ensure_sample_rate(sample_rate, wav_data)

    waveform = wav_data / tf.int16.max

    # rodar o modelo
    scores, embeddings, spectrogram = model(waveform)

    scores_np = scores.numpy()
    spectrogram_np = spectrogram.numpy()
    infered_class = class_names[scores_np.mean(axis=0).argmax()]
    print(f'O som principal é: {infered_class}')
    return infered_class



def process_audio(full_audio_data, sample_rate):
    # Concatena os dados de áudio acumulados
    full_audio_array = np.concatenate(full_audio_data)

    audio_no_noise = nr.reduce_noise(y = full_audio_array, sr=sample_rate)



    # Salva o áudio como arquivo WAV
    audio_path = "audios/received_audio.wav"
    save_audio_as_wav(audio_no_noise, sample_rate, audio_path)

   

    # Transcreve o áudio
    audio_class = classify_audio(audio_path)


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

    client_socket.settimeout(TIMEOUT)  # Define o tempo de espera 

    return client_socket, client_address

# Inicia o servidor
def receive_data():
    full_audio_data = []  # Armazenar os dados de áudio completos
    total_samples = 0

    client_socket, client_address = start_server()
    print(f"Conexão estabelecida com o cliente {client_address}.")

    data_buffer = b""

    while True:
        
        try:
              # Buffer para armazenar os pacotes recebidos

            packet = client_socket.recv(BUFFER_SIZE)

            if len(packet) == 2: 

                listen_flag2 = packet[0] + (packet[1] << 8)
              
                if listen_flag2 == 1:
                    print("Flag LISTEN recebida, processando o áudio acumulado...")
                    
                    audio_data = np.frombuffer(data_buffer, dtype=np.uint16)
                   

                    full_audio_data.append(audio_data)
                    total_samples += len(full_audio_data)

                    
                    
                    transcription = process_audio(full_audio_data, SAMPLE_RATE)
                   

                    client_socket.sendall(transcription[:15].encode("utf-8"))

                   
                    full_audio_data = []
                    total_samples = 0

                data_buffer = b""
            if not packet:
                raise ConnectionError("Conexão encerrada pelo cliente.")

            data_buffer += packet  # Acumula os pacotes recebidos


            print(f"dados recebidos: {len(packet)}")
            print(f"dados acumulados: {len(data_buffer)}")

                 
        except Exception as e:
            print(f"Erro: {e}")

    
if __name__ == '__main__':
    receive_data()
