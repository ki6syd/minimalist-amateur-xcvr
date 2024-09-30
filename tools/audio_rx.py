import socket
import wave
import struct
import pyaudio

# Set up TCP server parameters
HOST = '0.0.0.0'        # Listen on all available network interfaces
PORT = 7000             # Choose a port number
CHUNK_SIZE = 256        # Buffer size to receive audio data
TIMEOUT_DURATION = 5    # Timeout in seconds

# WAV file parameters
SAMPLE_RATE = 8000      # 8kHz sample rate
SAMPLE_WIDTH = 2        # 16-bit samples (2 bytes)
CHANNELS = 1            # Mono audio

# Initialize PyAudio for live playback
p = pyaudio.PyAudio()

# Set up audio stream for playback
stream = p.open(format=p.get_format_from_width(SAMPLE_WIDTH),
                channels=CHANNELS,
                rate=SAMPLE_RATE,
                output=True)

def save_audio_to_wav(file_name, raw_audio_data):
    """Save raw audio data to a WAV file."""
    with wave.open(file_name, 'wb') as wav_file:
        wav_file.setnchannels(CHANNELS)
        wav_file.setsampwidth(SAMPLE_WIDTH)
        wav_file.setframerate(SAMPLE_RATE)
        wav_file.writeframes(raw_audio_data)

def start_audio_server():
    """Start the TCP server to receive raw audio samples and play them live."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"Listening for connections on {HOST}:{PORT}...")

        conn, addr = s.accept()
        with conn:
            print(f"Connection established with {addr}")

            raw_audio_data = b''  # Buffer to hold the incoming audio data
            conn.settimeout(TIMEOUT_DURATION)  # Set timeout for 1 second of inactivity

            try:
                # Receive and play the audio data in chunks
                while True:
                    try:
                        data = conn.recv(CHUNK_SIZE)
                        if not data:
                            break  # No more data from client, break the loop
                        raw_audio_data += data
                        # Play the audio chunk live
                        stream.write(data)
                    except socket.timeout:
                        print("Connection timed out due to inactivity.")
                        break  # Break out of the loop if data is not received in time
            except Exception as e:
                print(f"An error occurred: {e}")
    
    # Save the received data as a WAV file
    if raw_audio_data:
        save_audio_to_wav("received_audio.wav", raw_audio_data)
        print("Audio data saved as 'received_audio.wav'.")
    else:
        print("No audio data received, file not created.")

if __name__ == "__main__":
    start_audio_server()

    # Stop and close the PyAudio stream after server stops
    stream.stop_stream()
    stream.close()
    p.terminate()