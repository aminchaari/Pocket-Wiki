import os
import time
import re
import wikipediaapi
from pathlib import Path

# ================= CONFIGURATION =================
INPUT_FILE = "all.txt"         # Your list of topics
ROOT_DIR = "wiki_archive_test" # Save folder
MAX_DEPTH = 10                 # Limit folder nesting
DELAY_SEC = 1.0                # Politeness delay
# =================================================

def clean_text_for_esp32(text):
    """
    Cleans text for ESP32: removes References, External Links, simplifies quotes.
    """
    # 1. Truncate at "See also", "References", or "External links"
    markers = ["\n== See also ==", "\n== References ==", "\n== External links =="]
    for marker in markers:
        if marker in text:
            text = text.split(marker)[0]
            
    # 2. Simplify characters (ESP32 fonts often lack curly quotes)
    text = text.replace('“', '"').replace('”', '"').replace("’", "'")
    
    # 3. Remove excessive newlines
    text = re.sub(r'\n{3,}', '\n\n', text)
    
    return text.strip()

def sanitize_filename(name):
    """
    Cleans the filename for the .txt file itsealf (keeps spaces).
    Example: "World War II" -> "World War II"
    """
    return re.sub(r'[<>:"/\\|?*]', '', name).strip()

def get_storage_path(root, topic_name):
    """
    Creates the nested folder path.
    Rule:
    - Letters (A-Z) -> Create folder with that letter.
    - Numbers (0-9) -> Create folder '#'.
    - Anything else (Spaces, -, /) -> IGNORE completely.
    """
    clean_path = []
    
    for char in topic_name:
        if char.isalpha():
            # If it's a letter (including Roman I, V, X), use it.
            clean_path.append(char.lower())
        elif char.isdigit():
            # If it's a number, turn it into '#'.
            clean_path.append('#')
        # Spaces, dashes, and slashes are skipped.
        
    # Handle empty edge case
    if not clean_path:
        clean_path = ["_misc"]
        
    # Apply Depth Limit
    if MAX_DEPTH and len(clean_path) > MAX_DEPTH:
        clean_path = clean_path[:MAX_DEPTH]
        
    # Build the path
    return Path(root).joinpath(*clean_path)

def download_topics():
    # Initialize API (No Auto-Suggest = Solves "Sun vs Son" issue)
    wiki = wikipediaapi.Wikipedia(
        user_agent='ESP32WikiTest/1.0 (contact: test@example.com)',
        language='en',
        extract_format=wikipediaapi.ExtractFormat.WIKI
    )

    try:
        with open(INPUT_FILE, 'r', encoding='utf-8') as f:
            topics = [line.strip() for line in f if line.strip()]
    except FileNotFoundError:
        print(f"Error: Could not find {INPUT_FILE}")
        return

    print(f"Found {len(topics)} topics. Starting download...")
    print(f"Mode: Exact Match | A-Z and # folders only | Lowercase Filenames")
    print("-" * 40)

    for i, topic in enumerate(topics):
        try:
            # 1. Fetch Page
            page = wiki.page(topic)
            
            # 2. Verify Existence
            if not page.exists():
                print(f"[{i+1}] Skipped (Not Found): '{topic}'")
                continue

            # 3. Clean Content
            content = clean_text_for_esp32(page.text)
            
            if not content:
                print(f"[{i+1}] Skipped (Empty Content): '{topic}'")
                continue

            # 4. Generate Paths
            folder_path = get_storage_path(ROOT_DIR, topic)
            
            # --- THE FIX: FORCE LOWERCASE HERE ---
            # Original Topic: "World War II" -> Cleaned: "world war ii" -> Final: "world war ii.txt"
            safe_filename = sanitize_filename(topic).lower() + ".txt"
            
            file_path = folder_path / safe_filename

            # 5. Create Directories & Save
            folder_path.mkdir(parents=True, exist_ok=True)
            
            # Check if file exists to save time on restarts
            if file_path.exists():
                print(f"[{i+1}] Skipped (Exists): {safe_filename}")
                continue

            with open(file_path, 'w', encoding='utf-8') as out_file:
                out_file.write(content)

            print(f"[{i+1}] Downloaded: {topic} -> {safe_filename}")
            print(f"      Path: {folder_path}")

            time.sleep(DELAY_SEC)

        except Exception as e:
            print(f"[{i+1}] Error processing '{topic}': {e}")

if __name__ == "__main__":
    download_topics()