# Pocket-Wiki

An offline, pocket-sized Wikipedia reader built on the ESP32. It allows you to search and read up to 100,000 Wikipedia articles entirely offline, loading text files instantly from an SD card using a custom directory indexing system.

## Hardware Requirements

To build this project, you will need the following components:
* **Microcontroller:** ESP32 Development Board
* **Display:** 2.4-inch SPI TFT LCD Touch Screen Module (Driver: ILI9341, SKU: MSP2402 - must include the built-in SD card reader)
* **Storage:** MicroSD Card (At least 8GB, formatted to FAT32)
* **Misc:** Breadboard and standard jumper wires



### Pin Wiring
Based on the code configuration, connect your ESP32 to the display/SD module using these pins (alongside standard 3.3V/GND and default hardware SPI pins):
* **TFT_CS (Display Chip Select):** Pin 5
* **SD_CS (SD Card Chip Select):** Pin 22
* **TFT_LED (Backlight):** Pin 15
* **T_IRQ (Touch Interrupt):** Pin 33

---

## Software Setup (PlatformIO)

This project is built using PlatformIO. You do not need the Arduino IDE.

1. Install **Visual Studio Code (VS Code)**.
2. Go to the Extensions tab and install the **PlatformIO IDE** extension.
3. Clone or download this repository to your computer.
4. In VS Code, click `File` -> `Open Folder...` and select the repository folder.
5. PlatformIO will automatically read the `platformio.ini` file, set up the ESP32 environment, and download necessary libraries like `TFT_eSPI`.
6. Plug in your ESP32 via USB and click the **Upload** arrow at the bottom of the VS Code window to flash the firmware.

*(Note: Ensure your `include/User_Setup.h` for the TFT_eSPI library matches your specific wiring and ILI9341 driver settings).*

---

## Generating the SD Card Data

The Pocket Wiki requires Wikipedia articles to be downloaded as plain text files and organized into a highly specific folder structure (e.g., `/w/o/r/l/d/world.txt`) so the ESP32 can load them instantly without crashing. 

To generate this database, you will use the two included Python scripts: `make_topics.py` and `downloadwiki.py`.

### Step 1: Download the Raw Pageviews Data
First, you need a list of the most popular Wikipedia articles.
1. Go to the official [Wikimedia Pageviews Dumps](https://dumps.wikimedia.org/other/pageviews/).
2. Navigate to a recent year and month.
3. Download one of the hourly `.gz` files (e.g., `pageviews-202X...gz`).
4. Extract the `.gz` file to get the raw text file containing the traffic data.

### Step 2: Extract the Top 100,000 Topics (`make_topics.py`)
The raw pageviews file contains millions of entries across all languages. This script filters it down to what we actually need.
* **How it works:** Run this script and point it to your extracted pageviews file. It will filter out non-English articles, sort them by view count, and isolate the top 100,000 most popular topics.
* **The Output:** It generates a clean `topics.txt` file containing just the titles of those top 100,000 articles, one per line.

### Step 3: Download and Format the Articles (`downloadwiki.py`)
Now that you have your list of topics, this script handles the actual downloading and formatting for the SD card.
* **How it works:** Run this script in the same directory as your `topics.txt` file. It will read the file line by line, connect to Wikipedia to download the plain text of each article, and automatically build the nested folder structure.
* **The Output:** A massive root folder containing all 100,000 `.txt` files properly sorted into nested A-Z and 0-9 directories. 
* **Final Step:** Simply drag and drop the contents of this output folder directly onto the root of your FAT32-formatted SD card.

---

## Usage
Once the ESP32 is flashed and the SD card is inserted:
1. Boot up the device.
2. Use the on-screen keyboard to type a search query.
3. Press **GO (!)** to load the article from the SD card.
4. Tap **NEXT** to paginate through the text, or **EXIT** to return to the search screen. 
5. The device will automatically enter Deep Sleep after 30 seconds of inactivity to save battery. Tapping the screen wakes it up exactly where you left off.
