from vosk import Model, KaldiRecognizer
import wave
import json
#from pydub import AudioSegment
import noisereduce as nr
import scipy.io.wavfile as wavfile
import numpy as np
#from pydub import AudioSegment
#from pydub.effects import low_pass_filter
import librosa
from transformers import pipeline

#from speechbrain.inference import LanguageIdentification
from speechbrain.inference.ASR import EncoderDecoderASR

asr_model = EncoderDecoderASR.from_hparams(source="speechbrain/asr-crdnn-rnnlm-librispeech", savedir="pretrained_models/asr-crdnn-rnnlm-librispeech")
#asr_model.transcribe_file('speechbrain/asr-crdnn-rnnlm-librispeech/example.wav')
# Baixa e carrega o modelo pré-treinado
#lang_id = LanguageIdentification.from_hparams(
 #   source="speechbrain/lang-id-commonvoice",
  #  savedir="tmp/lang-id-commonvoice"
#)



#import whisper


# Carrega o modelo de detecção de idiomas

#import sounddevice as sd
#from IPython.display import Audio
# Ler o arquivo .wav

# Aplicar redução de ruído
# é um algoritmo que utiliza a técnica Spectral gating


# Caminho para o modelo de português do Vosk
MODEL_PATH = "vosk-model-small-pt-0.3"  # O modelo baixado e descompactado

# Caminho do arquivo de áudio local
AUDIO_PATH = "audios/received_audio.wav"  # Substitua com o caminho do seu arquivo de áudio

# Carregar o modelo
#model = Model(MODEL_PATH)

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
    audio = audio.set_frame_rate(22050)  # 16 kHz
    
    # Salvar o áudio convertido
    audio.export(output_path, format="wav")


if __name__ == '__main__':

    #convert_audio("record_out.wav", "converted_audio.wav")

    sample_rate, audio = wavfile.read(AUDIO_PATH)


    audio_repeated = np.tile(audio, 5)

    audio_no_noise = nr.reduce_noise(y = audio_repeated, sr=sample_rate)

    output_path = "audios/audio_no_noise.wav"
    wavfile.write(output_path, sample_rate, audio_no_noise.astype('int16'))

    #convert_audio("audio_no_noise.wav","eu_te_amo_16k.wav")

    #transcription = transcribe_audio("audios/audio_no_noise_librosa.wav")
    #print("Transcrição: ", transcription)
    
    #model = whisper.load_model("base")
    #result = model.transcribe("audios/eu_te_amo_16k.wav", task="language")
    #print(f"Idioma detectado: {result['language']}")

    asr_model.transcribe_file('audios/eu_te_amo_16k.wav')
