"""
ESP32 Droid Voice Generator

Generates optimized robot voice clips for ESP32 flash storage.
Uses Microsoft Edge TTS for more expressive voices.
Outputs: 16kHz, 8-bit unsigned mono WAV (~16KB per second)

Usage:
    python voice_generator.py                    # Generate all phrases
    python voice_generator.py "Hello world"     # Generate single phrase
"""

import os
import sys
import asyncio
import warnings
from pathlib import Path
from typing import Optional
import numpy as np

# Suppress librosa warnings about short audio chunks
warnings.filterwarnings("ignore", message="n_fft=.*is too large")

try:
    import edge_tts
    import librosa
    import soundfile as sf
    from scipy.io import wavfile
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install with: pip install edge-tts librosa soundfile scipy numpy")
    sys.exit(1)


# === CONFIGURATION ===
OUTPUT_DIR = Path("./droid_sounds")
SAMPLE_RATE = 16000      # 16kHz output
PITCH_SHIFT_STEPS = 8    # Shift pitch up for robot sound (6-8 is Cozmo-like)
SPEED_RATE = 1.12        # Slightly faster for energy

# Edge TTS voice - try these for different feels:
# "en-US-GuyNeural" - male, friendly
# "en-US-ChristopherNeural" - male, warm  
# "en-US-EricNeural" - male, cheerful
# "en-US-JennyNeural" - female, friendly
VOICE = "en-US-EricNeural"

# Inflection settings
ADD_END_INFLECTION = True
INFLECTION_AMOUNT = 5      # Semitones to bend up at end
INFLECTION_DURATION = 0.15 # Last 15% of audio gets bent up


# === PHRASE LIBRARY ===
PHRASES = {
    "happy": [
        "Hahaha!",
        "Yay!",
        #"That tickles!",
        #"Ooh I like that!",
        "Weeee!",
        #"That's nice!",
        #"Hee hee!",
        "Oh boy!",
        "Yippeeeee!",
        "Mmm!",
    ],
    
    "annoyed": [
        "Hey!",
        "Stop it!",
        "Okay okay!",
        "Enough!",
        "Hmph!",
        "Cut it out!",
        "Ugh!",
        "Quit it!",
        "Grrr!",
        "No no no!",
    ],
    
    "surprised": [
        "Whoa!",
        "Huhh?",
        "What the what?",
        "Oh!",
        "Eep!",
        "Eak!",
        "Hey!",
        "Woah!",
    ],
    
    "sleepy": [
        "So sleepy...",
        "Yawn...",
        "Night night...",
        "Getting dark...",
        "Sleepy time...",
        #"Zzz...",
    ],
    
    "wake": [
        "Good morning!",
        "I'm awake!",
        "Hello!",
        "Hi there!",
        "Rise and shine!",
        "Hey hey!",
        "Waking up!",
        "Oh hi!",
    ],
    
    "idle": [
        "Hmm...",
        "Doo dee doo...",
        "La la la...",
        "Boop!",
        "Beep boop!",
        "Hello?",
        "Hmm hmm...",
        "Dum dee dum...",
        "Boo!",
        "Hm?",
    ],
    
    "excited": [
        "Wow!",
        "Woohoo!",
        "Amazing!",
        "Let's go!",
        "Yeah!",
        "Awesome!",
        "Oh yeah!",
        "Woo!",
        "Yes!",
        "Cool!",
    ],
}


class DroidVoiceGenerator:
    """Generates optimized robot voice clips for ESP32"""
    
    def __init__(self):
        self._temp_dir = Path("./temp_audio")
        self._temp_dir.mkdir(exist_ok=True)
        print(f"Using voice: {VOICE}")
        print(f"Pitch shift: +{PITCH_SHIFT_STEPS} semitones")
        if ADD_END_INFLECTION:
            print(f"End inflection: +{INFLECTION_AMOUNT} semitones")
    
    async def _generate_raw_tts(self, text: str, output_path: Path) -> bool:
        """Generate TTS using Edge TTS"""
        try:
            communicate = edge_tts.Communicate(text, VOICE)
            await communicate.save(str(output_path))
            return True
        except Exception as e:
            print(f"TTS failed: {e}")
            return False
    
    def _add_inflection(self, y: np.ndarray, sr: int) -> np.ndarray:
        """Add upward pitch bend at the end of the audio"""
        if not ADD_END_INFLECTION:
            return y
        
        # Calculate where inflection starts
        inflection_samples = int(len(y) * INFLECTION_DURATION)
        if inflection_samples < 100:
            return y
        
        # Split audio
        main_part = y[:-inflection_samples]
        end_part = y[-inflection_samples:]
        
        # Create pitch bend envelope (0 to INFLECTION_AMOUNT)
        bend_envelope = np.linspace(0, INFLECTION_AMOUNT, len(end_part))
        
        # Apply gradual pitch shift to end part
        # We'll do this by processing small chunks with increasing pitch
        chunk_size = len(end_part) // 10
        processed_end = []
        
        for i in range(10):
            start = i * chunk_size
            end = start + chunk_size if i < 9 else len(end_part)
            chunk = end_part[start:end]
            
            # Average pitch shift for this chunk
            avg_shift = bend_envelope[start:end].mean()
            
            if len(chunk) > 100:
                shifted = librosa.effects.pitch_shift(chunk, sr=sr, n_steps=avg_shift)
                processed_end.append(shifted)
            else:
                processed_end.append(chunk)
        
        end_processed = np.concatenate(processed_end)
        
        return np.concatenate([main_part, end_processed])
    
    def _apply_robot_effects(self, input_path: str) -> Optional[np.ndarray]:
        """Apply pitch shift and effects"""
        try:
            # Load audio
            y, sr = librosa.load(input_path, sr=None)
            
            # Speed up slightly for more energy
            y = librosa.effects.time_stretch(y, rate=SPEED_RATE)
            
            # Main pitch shift for robot sound
            y = librosa.effects.pitch_shift(y, sr=sr, n_steps=PITCH_SHIFT_STEPS)
            
            # Add end inflection
            y = self._add_inflection(y, sr)
            
            # Resample to target rate
            y = librosa.resample(y, orig_sr=sr, target_sr=SAMPLE_RATE)
            
            # Normalize
            y = librosa.util.normalize(y)
            
            return y
        except Exception as e:
            print(f"Effects processing failed: {e}")
            return None
    
    def _save_optimized(self, audio: np.ndarray, output_path: Path):
        """Save as 8-bit unsigned WAV for ESP32"""
        audio_clipped = np.clip(audio, -1, 1)
        audio_uint8 = ((audio_clipped + 1) * 127.5).astype(np.uint8)
        wavfile.write(str(output_path), SAMPLE_RATE, audio_uint8)
        
        size_kb = output_path.stat().st_size / 1024
        duration = len(audio) / SAMPLE_RATE
        print(f"    -> {output_path.name} ({size_kb:.1f}KB, {duration:.1f}s)")
    
    async def generate_clip(self, text: str, output_path: Path) -> bool:
        """Generate a single voice clip"""
        temp_path = self._temp_dir / "temp_tts.mp3"
        
        # Step 1: Generate TTS
        if not await self._generate_raw_tts(text, temp_path):
            return False
        
        # Step 2: Apply effects
        audio = self._apply_robot_effects(str(temp_path))
        if audio is None:
            return False
        
        # Step 3: Save optimized
        self._save_optimized(audio, output_path)
        
        # Cleanup temp
        if temp_path.exists():
            temp_path.unlink()
        
        return True
    
    async def generate_all_phrases(self):
        """Generate all phrases from the library"""
        OUTPUT_DIR.mkdir(exist_ok=True)
        
        total_size = 0
        total_clips = 0
        
        for category, phrases in PHRASES.items():
            print(f"\n=== {category.upper()} ===")
            category_dir = OUTPUT_DIR / category
            category_dir.mkdir(exist_ok=True)
            
            for i, phrase in enumerate(phrases):
                filename = f"{category}_{i+1:02d}.wav"
                output_path = category_dir / filename
                
                print(f"  [{i+1:02d}] \"{phrase}\"")
                if await self.generate_clip(phrase, output_path):
                    total_size += output_path.stat().st_size
                    total_clips += 1
        
        print(f"\n{'='*40}")
        print(f"Generated {total_clips} clips")
        print(f"Total size: {total_size/1024:.1f}KB ({total_size/1024/1024:.2f}MB)")
        print(f"Output directory: {OUTPUT_DIR.absolute()}")
        print(f"\nCopy contents to your Arduino sketch's 'data' folder")
        print(f"Then upload using 'ESP32 LittleFS Data Upload'")
    
    def cleanup(self):
        """Remove temp files"""
        for f in self._temp_dir.glob("*"):
            f.unlink()
        try:
            self._temp_dir.rmdir()
        except:
            pass


async def main():
    generator = DroidVoiceGenerator()
    
    if len(sys.argv) > 1:
        # Generate single phrase from command line
        text = " ".join(sys.argv[1:])
        OUTPUT_DIR.mkdir(exist_ok=True)
        safe_name = "".join(c if c.isalnum() else "_" for c in text[:30])
        output_path = OUTPUT_DIR / f"custom_{safe_name}.wav"
        print(f"Generating: \"{text}\"")
        await generator.generate_clip(text, output_path)
    else:
        # Generate all phrases
        print("ESP32 Droid Voice Generator")
        print(f"Output: {SAMPLE_RATE}Hz, 8-bit mono (~16KB/sec)")
        print("="*40)
        await generator.generate_all_phrases()
    
    generator.cleanup()


if __name__ == "__main__":
    asyncio.run(main())
