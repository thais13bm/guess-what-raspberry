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
import winsound
#from speechbrain.inference import LanguageIdentification

#import torchaudio
#from torchaudio.pipelines import YAMNET_CLASS_MAP, YAMNET_PRETRAINED_MODEL
import scipy

import tensorflow as tf
import tensorflow_hub as hub
import numpy as np
import csv

import matplotlib.pyplot as plt
#from IPython.display import Audio
from scipy.io import wavfile

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
AUDIO_PATH = "audios/record_out (4).wav"  # Substitua com o caminho do seu arquivo de áudio

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


if __name__ == '__main__':

    #convert_audio("record_out.wav", "converted_audio.wav")


    # Load the model.
    model = hub.load('https://tfhub.dev/google/yamnet/1')

    class_map_path = model.class_map_path().numpy()
    class_names = class_names_from_csv(class_map_path)

    wav_file_name = AUDIO_PATH
    sample_rate, wav_data = wavfile.read(wav_file_name, 'rb')
    sample_rate, wav_data = ensure_sample_rate(sample_rate, wav_data)

    waveform = wav_data / tf.int16.max

    # Run the model, check the output.
    scores, embeddings, spectrogram = model(waveform)

    scores_np = scores.numpy()
    spectrogram_np = spectrogram.numpy()
    infered_class = class_names[scores_np.mean(axis=0).argmax()]
    print(f'The main sound is: {infered_class}')

    if(False):
        sample_rate, audio = wavfile.read(AUDIO_PATH)


        audio_repeated = np.tile(audio, 4)

        #audio_no_noise = nr.reduce_noise(y = audio_repeated, sr=sample_rate)

        output_path = "audios/audio_repeated.wav"
        wavfile.write(output_path, sample_rate, audio_repeated.astype('int16'))

        #convert_audio("audio_no_noise.wav","eu_te_amo_16k.wav")

        #winsound.PlaySound(output_path, winsound.SND_FILENAME)

        transcription = transcribe_audio("audios/audio_repeated.wav")
        print("Transcrição: ", transcription)
    
        #model = whisper.load_model("base")
        #result = model.transcribe("audios/eu_te_amo_16k.wav", task="language")
        #print(f"Idioma detectado: {result['language']}")

        #asr_model.transcribe_file('audios/eu_te_amo_16k.wav')
        # Carregar o modelo YAMNet pré-treinado
        model = torchaudio.pipelines.YAMNET_PRETRAINED_MODEL.get_model()

        # Entrada: Amostra de áudio
        waveform, sample_rate = torchaudio.load("audios/received_audio.wav")
        waveform = torchaudio.functional.resample(waveform, sample_rate, model.sample_rate)

        # Inferência
        outputs, embeddings, spectrogram = model(waveform)
        predictions = outputs.argmax(dim=1)

        # Obter a classe do som
        sound_class = YAMNET_CLASS_MAP[predictions.item()]
        print(f"Som detectado: {sound_class}")
