# GuessWhat: Audio Classification IoT for the Hearing Impaired

The **GuessWhat** project consists of a distributed system for classifying sounds captured by the BitDogLab board's microphone. The target audience primarily includes individuals with hearing impairments, who can benefit from receiving descriptions of the surrounding sounds. BitDogLab is a development kit that includes a Raspberry Pi Pico W, also known as the RP2040.

The GuessWhat system is capable of capturing 3-second audio snippets, sending them via Wi-Fi to a server, which then pre-processes the sound and returns a classification — for example, whether the sound is speech, music, crying, or animal noise.

## 🚀 How to Run

### Wi-fi connectivity

- Edit the code to include the correct IPs and Wi-Fi credentials before running the project.


### Start the server
```bash
python3 server_tcp.py
```

### Upload the firmware to the Raspberry Pi Pico W
- Compile the firmware using the Pico SDK.
- Flash it via USB 

### Additional information

- You can find more details about hardware connection/requirements, motivation and code explanation in the PDF report. 








