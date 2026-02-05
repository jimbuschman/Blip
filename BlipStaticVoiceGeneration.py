"""
ESP32 Droid Voice Generator

Generates optimized robot voice clips for ESP32 flash storage.
Outputs: 16kHz, 8-bit unsigned mono WAV (~16KB per second)

Usage:
    python voice_generator.py                    # Generate all phrases
    python voice_generator.py "Hello world"     # Generate single phrase
"""

import os
import sys
from pathlib import Path
from typing import Optional
import numpy as np

try:
    import pyttsx3
    import librosa
    import soundfile as sf
    from scipy.io import wavfile
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install with: pip install pyttsx3 librosa soundfile scipy")
    sys.exit(1)


# === CONFIGURATION ===
OUTPUT_DIR = Path("./droid_sounds")
SAMPLE_RATE = 16000      # 16kHz (half of original, saves space)
BIT_DEPTH = 8            # 8-bit (half of 16-bit, saves more space)
PITCH_SHIFT_STEPS = 4    # Shift pitch up for robot sound
SPEECH_RATE = 150        # Words per minute


# === PHRASE LIBRARY ===
# Organized by category to match ESP32 droid firmware
# Keep phrases SHORT (under 2 seconds) to save space

PHRASES = {
    # Touch top, pet mode, scoring in Pong
    "happy": [
        "Hehe!",
        "Yay!",
        "That tickles!",
        "Ooh I like that!",
        "Wee!",
        "That's nice!",
        "Hee hee!",
        "Oh boy!",
        "Yippee!",
        "Mmm!",
    ],
    
    # Touch spam, AI scores in Pong, Simon game over
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
    
    # Touch back (startled)
    "surprised": [
        "Whoa!",
        "Huh?",
        "What the?",
        "Oh!",
        "Eep!",
        "Ahh!",
        "Hey!",
        "Woah!",
    ],
    
    # Going to sleep (dark detected)
    "sleepy": [
        "So sleepy...",
        "Yawn...",
        "Night night...",
        "Getting dark...",
        "Sleepy time...",
        "Zzz...",
    ],
    
    # Waking up, startup
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
    
    # Random idle chirps
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
    
    # Dance, game menu, winning games
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
        "Haha!",
    ],
}


class DroidVoiceGenerator:
    """Generates optimized robot voice clips for ESP32"""
    
    def __init__(self):
        self._engine = None
        self._temp_dir = Path("./temp_audio")
        self._temp_dir.mkdir(exist_ok=True)
        self._init_engine()
    
    def _init_engine(self):
        """Initialize TTS engine"""
        try:
            self._engine = pyttsx3.init()
            self._engine.setProperty('rate', SPEECH_RATE)
            
            # Try to find a good voice
            voices = self._engine.getProperty('voices')
            for voice in voices:
                if 'david' in voice.name.lower() or 'male' in voice.name.lower():
                    self._engine.setProperty('voice', voice.id)
                    print(f"Using voice: {voice.name}")
                    break
            
            print("TTS engine initialized")
        except Exception as e:
            print(f"Failed to init TTS: {e}")
            self._engine = None
    
    def _generate_raw_tts(self, text: str) -> Optional[str]:
        """Generate raw TTS to temp file"""
        if not self._engine:
            return None
        
        temp_path = self._temp_dir / "raw.wav"
        try:
            self._engine.save_to_file(text, str(temp_path))
            self._engine.runAndWait()
            return str(temp_path)
        except Exception as e:
            print(f"TTS failed: {e}")
            return None
    
    def _apply_robot_effects(self, input_path: str) -> Optional[np.ndarray]:
        """Apply pitch shift and effects, return processed audio"""
        try:
            # Load audio
            y, sr = librosa.load(input_path, sr=None)
            
            # Pitch shift up for robot sound
            y_shifted = librosa.effects.pitch_shift(y, sr=sr, n_steps=PITCH_SHIFT_STEPS)
            
            # Slight time stretch for robotic feel
            y_stretched = librosa.effects.time_stretch(y_shifted, rate=1.1)
            
            # Resample to target rate
            y_resampled = librosa.resample(y_stretched, orig_sr=sr, target_sr=SAMPLE_RATE)
            
            # Normalize
            y_normalized = librosa.util.normalize(y_resampled)
            
            return y_normalized
        except Exception as e:
            print(f"Effects processing failed: {e}")
            return None
    
    def _save_optimized(self, audio: np.ndarray, output_path: Path):
        """Save as 8-bit unsigned WAV for ESP32"""
        # Convert float [-1, 1] to uint8 [0, 255]
        # 8-bit WAV uses unsigned with 128 as center
        audio_clipped = np.clip(audio, -1, 1)
        audio_uint8 = ((audio_clipped + 1) * 127.5).astype(np.uint8)
        
        # Write WAV file
        wavfile.write(str(output_path), SAMPLE_RATE, audio_uint8)
        
        # Report size
        size_kb = output_path.stat().st_size / 1024
        duration = len(audio) / SAMPLE_RATE
        print(f"    -> {output_path.name} ({size_kb:.1f}KB, {duration:.1f}s)")
    
    def generate_clip(self, text: str, output_path: Path) -> bool:
        """Generate a single voice clip"""
        # Step 1: Raw TTS
        raw_path = self._generate_raw_tts(text)
        if not raw_path:
            return False
        
        # Step 2: Apply effects
        audio = self._apply_robot_effects(raw_path)
        if audio is None:
            return False
        
        # Step 3: Save optimized
        self._save_optimized(audio, output_path)
        return True
    
    def generate_all_phrases(self):
        """Generate all phrases from the library"""
        OUTPUT_DIR.mkdir(exist_ok=True)
        
        total_size = 0
        total_clips = 0
        
        for category, phrases in PHRASES.items():
            print(f"\n=== {category.upper()} ===")
            category_dir = OUTPUT_DIR / category
            category_dir.mkdir(exist_ok=True)
            
            for i, phrase in enumerate(phrases):
                # Simple filename to match ESP32 firmware expectations
                # e.g., happy_01.wav, happy_02.wav
                filename = f"{category}_{i+1:02d}.wav"
                output_path = category_dir / filename
                
                print(f"  [{i+1:02d}] \"{phrase}\"")
                if self.generate_clip(phrase, output_path):
                    total_size += output_path.stat().st_size
                    total_clips += 1
        
        print(f"\n{'='*40}")
        print(f"Generated {total_clips} clips")
        print(f"Total size: {total_size/1024:.1f}KB ({total_size/1024/1024:.2f}MB)")
        print(f"Output directory: {OUTPUT_DIR.absolute()}")
        print(f"\nUpload the '{OUTPUT_DIR.name}' folder contents to ESP32 LittleFS")
    
    def cleanup(self):
        """Remove temp files"""
        for f in self._temp_dir.glob("*"):
            f.unlink()
        try:
            self._temp_dir.rmdir()
        except:
            pass


def main():
    generator = DroidVoiceGenerator()
    
    if len(sys.argv) > 1:
        # Generate single phrase from command line
        text = " ".join(sys.argv[1:])
        OUTPUT_DIR.mkdir(exist_ok=True)
        safe_name = "".join(c if c.isalnum() else "_" for c in text[:30])
        output_path = OUTPUT_DIR / f"custom_{safe_name}.wav"
        print(f"Generating: \"{text}\"")
        generator.generate_clip(text, output_path)
    else:
        # Generate all phrases
        print("ESP32 Droid Voice Generator")
        print(f"Output format: {SAMPLE_RATE}Hz, {BIT_DEPTH}-bit mono")
        print(f"~{SAMPLE_RATE * BIT_DEPTH // 8 // 1024}KB per second of audio")
        generator.generate_all_phrases()
    
    generator.cleanup()


if __name__ == "__main__":
    main()
