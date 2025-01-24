import numpy as np
import matplotlib.pyplot as plt
from scipy.io.wavfile import write


vet = np.load("audio_data.npy")

print(vet)


write("example.wav", 22050, vet.astype(np.int16))
#plt.figure()
#plt.plot(vet)