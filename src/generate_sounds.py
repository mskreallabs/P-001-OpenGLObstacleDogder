import wave
import struct
import math
import os

def create_sound(filename, frequency, duration, volume=0.5, fade_out=False):
    """Create a simple WAV sound effect"""
    sample_rate = 44100
    num_samples = int(sample_rate * duration)
    
    filepath = os.path.join("sounds", filename)
    
    with wave.open(filepath, 'w') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        
        for i in range(num_samples):
            if fade_out:
                current_volume = volume * (1.0 - i / num_samples)
            else:
                current_volume = volume
                
            value = int(current_volume * 32767 * math.sin(2 * math.pi * frequency * i / sample_rate))
            data = struct.pack('<h', value)
            wav_file.writeframes(data)
    
    print(f"Created: {filepath}")

# Create sounds folder FIRST
print("Creating sounds folder...")
os.makedirs("sounds", exist_ok=True)
print("sounds folder ready!\n")

print("Generating sound effects...\n")

# 1. Background music (30 seconds ambient loop)
print("Creating background music...")
sample_rate = 44100
duration = 30.0
num_samples = int(sample_rate * duration)

with wave.open("sounds/background.wav", 'w') as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(sample_rate)
    
    for i in range(num_samples):
        t = i / sample_rate
        freq1 = 220 + 50 * math.sin(2 * math.pi * 0.5 * t)
        freq2 = 330 + 30 * math.sin(2 * math.pi * 0.3 * t)
        value = int(0.3 * 32767 * (math.sin(2 * math.pi * freq1 * t) + math.sin(2 * math.pi * freq2 * t)) / 2)
        data = struct.pack('<h', value)
        wav_file.writeframes(data)
print("Created: sounds/background.wav")

# 2. Collision sound (low boom)
print("Creating collision sound...")
create_sound("collision.wav", 150, 0.4, 0.7, fade_out=True)

# 3. Point scored (rising tone)
print("Creating point sound...")
sample_rate = 44100
duration = 0.2
num_samples = int(sample_rate * duration)

with wave.open("sounds/point.wav", 'w') as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(sample_rate)
    
    for i in range(num_samples):
        t = i / sample_rate
        freq = 440 + 440 * t
        volume = 0.6 * (1.0 - t)
        value = int(volume * 32767 * math.sin(2 * math.pi * freq * t))
        data = struct.pack('<h', value)
        wav_file.writeframes(data)
print("Created: sounds/point.wav")

# 4. Game start (happy sequence)
print("Creating start sound...")
sample_rate = 44100
duration = 0.5
num_samples = int(sample_rate * duration)

with wave.open("sounds/start.wav", 'w') as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(sample_rate)
    
    for i in range(num_samples):
        t = i / sample_rate
        if t < 0.15:
            freq = 523  # C5
        elif t < 0.3:
            freq = 659  # E5
        else:
            freq = 784  # G5
        volume = 0.6
        value = int(volume * 32767 * math.sin(2 * math.pi * freq * t))
        data = struct.pack('<h', value)
        wav_file.writeframes(data)
print("Created: sounds/start.wav")

print("\n" + "="*50)
print("All sound effects generated successfully!")
print("="*50)
print(f"\nSound files are in: {os.path.abspath('sounds')}")
print("\nFiles created:")
for file in os.listdir("sounds"):
    size = os.path.getsize(os.path.join("sounds", file))
    print(f"  - {file} ({size:,} bytes)")