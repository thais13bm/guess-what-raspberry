from flask import Flask, request, jsonify
import os
import wave
import json
import numpy as np
from vosk import Model, KaldiRecognizer

app = Flask(__name__)

# Caminho para o modelo de português do Vosk
MODEL_PATH = "vosk-model-small-pt-0.3"  # O modelo baixado e descompactado

# Carregar o modelo
model = Model(MODEL_PATH)

def save_audio_as_wav(audio_data, sample_rate, filename="audio.wav"):
    # Criar um arquivo .wav com as informações do áudio
    with wave.open(filename, 'wb') as wf:
        wf.setnchannels(1)  # Canal mono
        wf.setsampwidth(2)  # 2 bytes (16 bits) por amostra
        wf.setframerate(sample_rate)
        # Converter o vetor de áudio para bytes
        wf.writeframes(audio_data.tobytes())

@app.route('/upload', methods=['POST'])
def upload_audio():
    try:
        # Recebe o vetor de áudio enviado pela Raspberry (no formato JSON, por exemplo)
        audio_data = request.json.get('audio')  # Espera um vetor JSON com as amostras
        sample_rate = request.json.get('sample_rate', 16000)  # Exemplo de taxa de amostragem, ajustar conforme necessário

        # Converte o vetor de áudio em um arquivo WAV
        audio_np = np.array(audio_data, dtype=np.int16)  # Converte para o formato correto (16-bit PCM)
        audio_path = "received_audio.wav"
        save_audio_as_wav(audio_np, sample_rate, audio_path)

        # Converte o áudio para texto
        transcription = transcribe_audio(audio_path)
        return jsonify({"text": transcription}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

def transcribe_audio(audio_path):
    # Abre o arquivo de áudio
    with wave.open(audio_path, "rb") as audio_file:
        frame_rate = audio_file.getframerate()
        audio_content = audio_file.readframes(audio_file.getnframes())

    # Inicializa o reconhecedor do Vosk
    recognizer = KaldiRecognizer(model, frame_rate)

    recognizer.AcceptWaveform(audio_content)
    result = recognizer.Result()
    result_json = json.loads(result)
    transcription = result_json.get("text", "")

    print(transcription)

    return transcription

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
