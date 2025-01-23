from vosk import Model, KaldiRecognizer
import wave
import json
from pydub import AudioSegment



# Caminho para o modelo de português do Vosk
MODEL_PATH = "vosk-model-small-pt-0.3"  # O modelo baixado e descompactado

# Caminho do arquivo de áudio local
AUDIO_PATH = "record_out (3).wav"  # Substitua com o caminho do seu arquivo de áudio

# Carregar o modelo
model = Model(MODEL_PATH)

def transcribe_audio(audio_path):
    # Abre o arquivo de áudio
    with wave.open(audio_path, "rb") as audio_file:
        frame_rate = audio_file.getframerate()
        print("frame rate", frame_rate)
        audio_content = audio_file.readframes(audio_file.getnframes())
        
    # Inicializa o reconhecedor do Vosk
    recognizer = KaldiRecognizer(model, frame_rate)
    
    recognizer.AcceptWaveform(audio_content)
    result = recognizer.Result()
    result_json = json.loads(result)
    transcription = result_json.get("text", "")


    # Inicia o reconhecimento do áudio
    #if recognizer.AcceptWaveform(audio_content):
     #   result = recognizer.Result()
      #  result_json = json.loads(result)
       # transcription = result_json.get("text", "")
    #else:
     #   transcription = "Não foi possível transcrever o áudio."

    return transcription


def convert_audio(input_path, output_path):
    # Carregar o arquivo de áudio
    audio = AudioSegment.from_wav(input_path)
    
    # Converter para mono, 16 kHz e 16-bit
    audio = audio.set_channels(1)  # Mono
    audio = audio.set_frame_rate(16000)  # 16 kHz
    
    # Salvar o áudio convertido
    audio.export(output_path, format="wav")


if __name__ == '__main__':

    #convert_audio("record_out.wav", "converted_audio.wav")
    transcription = transcribe_audio(AUDIO_PATH)
    print("Transcrição: ", transcription)
