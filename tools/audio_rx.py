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
        
def handle_client(conn, addr):
    """Handle the client connection, receive and play audio data."""
    print(f"Connection established with {addr}")
    
    # Set up audio stream for playback
    stream = p.open(format=p.get_format_from_width(SAMPLE_WIDTH),
                    channels=CHANNELS,
                    rate=SAMPLE_RATE,
                    output=True)

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
    finally:
        # Save the received data as a WAV file
        if raw_audio_data:
            save_audio_to_wav(f"received_audio_{addr[1]}.wav", raw_audio_data)
            print(f"Audio data saved as 'received_audio_{addr[1]}.wav'.")
        else:
            print("No audio data received, file not created.")
        
        # Stop and close the PyAudio stream after client disconnects
        stream.stop_stream()
        stream.close()
        conn.close()
        print(f"Connection with {addr} closed.")

def start_audio_server():
    """Start the TCP server to receive raw audio samples and play them live."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"Listening for connections on {HOST}:{PORT}...")

        while True:  # Loop to keep accepting new connections
            try:
                conn, addr = s.accept()
                handle_client(conn, addr)
            except Exception as e:
                print(f"An error occurred: {e}")
                break  # Stop server in case of a critical error

if __name__ == "__main__":
    try:
        start_audio_server()
    except KeyboardInterrupt:
        print("\nServer stopped manually.")
    finally:
        # Clean up PyAudio when the server stops
        p.terminate()