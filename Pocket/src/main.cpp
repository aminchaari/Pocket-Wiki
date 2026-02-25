#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>

// ==========================================
//   1. GLOBAL STATE & RTC MEMORY (DEEP SLEEP)
// ==========================================
RTC_DATA_ATTR enum AppState { KEYBOARD, LOADING, READING };
RTC_DATA_ATTR char savedSearchQuery[32] = ""; 
RTC_DATA_ATTR unsigned long savedFilePosition = 0;
RTC_DATA_ATTR char savedFilename[64] = "";

AppState currentState = KEYBOARD;

// ==========================================
//   2. HARDWARE & DISPLAY CONFIGURATION
// ==========================================
#define T_IRQ 33
#define TFT_CS_PIN 5
#define SD_CS_PIN 22
#define TFT_LED_PIN 15
#define MAX_DEPTH 10

TFT_eSPI tft = TFT_eSPI();
uint16_t calData[5] = { 421, 3387, 373, 3303, 7 };

// ==========================================
//   3. WIKI & TIMEOUT VARIABLES
// ==========================================
String searchQuery = "";
String currentFilename = ""; 
unsigned long filePosition = 0;
bool endOfFile = false; 

unsigned long lastActivityTime = 0;
const unsigned long SLEEP_TIMEOUT = 30000; 

// ==========================================
//   4. KEYBOARD LAYOUT CONFIGURATION
// ==========================================
const int keyW = 28;
const int keyH = 32;
const int keyGap = 3;
const int keyboardTopY = 90;

char keys[4][10] = {
  {'1','2','3','4','5','6','7','8','9','0'},
  {'q','w','e','r','t','y','u','i','o','p'},
  {'a','s','d','f','g','h','j','k','l','z'},
  {'w','x','c','v','b','n','m',' ','<','!'} 
};


// ==========================================
//   5. UTILITY & POWER MANAGEMENT FUNCTIONS
// ==========================================

// Called on every touch input to ensure the device stays awake while actively being used.
void resetTimer() { 
  lastActivityTime = millis(); 
}

// Backs up the search state and reading position into RTC memory so the user picks up exactly where they left off upon waking.
void goToSleep() {
  tft.fillScreen(TFT_BLACK);
  digitalWrite(TFT_LED_PIN, LOW); 

  searchQuery.toCharArray(savedSearchQuery, 32);
  currentFilename.toCharArray(savedFilename, 64);
  savedFilePosition = filePosition;

  esp_sleep_enable_ext0_wakeup((gpio_num_t)T_IRQ, 0); 
  delay(50);
  esp_deep_sleep_start();
}


// ==========================================
//   6. FILE SYSTEM & PATH ROUTING
// ==========================================

// Matches the specific folder structure on the SD card (e.g., /w/o/r/l/d/) so the system locates files quickly without heavy scanning.
String getPathFromTitle(String title) {
  String path = "/";
  int depth = 0;

  for (int i = 0; i < title.length(); i++) {
    char c = title.charAt(i);

    if (isAlpha(c)) {
      path += String((char)tolower(c));
      path += "/";
      depth++;
    } 
    else if (isDigit(c)) {
      path += "#/";
      depth++;
    }
    
    if (depth >= MAX_DEPTH) break;
  }
  
  if (path == "/") return "/_misc/";
  
  return path;
}

// Prevents file system crashes by ensuring the file name only contains safe, allowed characters before opening.
String cleanFilename(String name) {
  String cleaned = "";
  for (int i = 0; i < name.length(); i++) {
    char c = name.charAt(i);
    if (isAlphaNumeric(c) || c == ' ' || c == '.' || c == '-' || c == '_') {
      cleaned += c;
    }
  }
  return cleaned;
}


// ==========================================
//   7. DISPLAY & UI RENDERING
// ==========================================

// Sets up the visual interface for text input, color-coding special keys (DEL, GO, SPC) to make them easily distinguishable.
void drawKeyboard() {
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("POCKET WIKI v2.0", 10, 10, 2);
  tft.setTextSize(1);
  
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 10; col++) {
      int x = 5 + (col * (keyW + keyGap));
      int y = keyboardTopY + (row * (keyH + keyGap));
      
      uint16_t btnColor = TFT_DARKGREY;
      String label = String(keys[row][col]);

      if (keys[row][col] == '<') {
        btnColor = TFT_RED;
        label = "DEL";
      } else if (keys[row][col] == '!') {
        btnColor = TFT_GREEN;
        label = "GO";
      } else if(keys[row][col]==' ') {
        btnColor= TFT_YELLOW;
        label="SPC";
      }

      tft.fillRoundRect(x, y, keyW, keyH, 4, btnColor);
      tft.drawRoundRect(x, y, keyW, keyH, 4, TFT_WHITE);
      
      tft.setTextColor(TFT_WHITE);
      if (label.length() > 1) {
        tft.drawString(label, x + 5, y + 10);
      } else {
        tft.drawChar(keys[row][col], x + 10, y + 10, 2);
      }
    }
  }
}

// Provides immediate visual feedback when the user taps a key, updating the display seamlessly.
void updateSearchBar() {
  tft.fillRect(5, 40, 310, 40, TFT_NAVY);
  tft.drawRect(5, 40, 310, 40, TFT_WHITE);
  tft.setCursor(15, 52);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print(searchQuery);
  tft.print("_"); 
}

// Handles smart word-wrapping and pagination, saving the exact byte position so the next page picks up perfectly.
void displayNextPage() {
  tft.fillScreen(TFT_BLACK);
  
  tft.fillRect(0, 210, 100, 30, TFT_RED);
  tft.drawRect(0, 210, 100, 30, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("EXIT", 50, 210, 2);

  if (!endOfFile) {
    tft.fillRect(220, 210, 100, 30, TFT_GREEN);
    tft.drawRect(220, 210, 100, 30, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.drawCentreString("NEXT", 270, 210, 2);
  }

  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2); 

  File f = SD.open(currentFilename);
  if (!f) return;

  f.seek(filePosition);

  String currentWord = "";
  int rightMargin = 310;
  int bottomMargin = 190;

  while (f.available()) {
    char c = f.read();
    
    if (c == ' ' || c == '\n') {
      int wordWidth = tft.textWidth(currentWord);
      
      if (tft.getCursorX() + wordWidth > rightMargin) {
        tft.println();
      }
      
      if (tft.getCursorY() > bottomMargin) {
        filePosition = f.position() - currentWord.length() - 1; 
        f.close();
        return; 
      }

      tft.print(currentWord);
      if (c == '\n') tft.println();
      else tft.print(" ");

      currentWord = "";
    } 
    else if (c >= 33 && c <= 126) { 
      currentWord += c;
    }
  }

  if (currentWord.length() > 0) {
      if (tft.getCursorX() + tft.textWidth(currentWord) > rightMargin) tft.println();
      tft.print(currentWord);
  }

  endOfFile = true;
  f.close();
  
  tft.fillRect(220, 210, 100, 30, TFT_BLACK); 
}


// ==========================================
//   8. CORE LOGIC & EVENT HANDLERS
// ==========================================

// Acts as the bridge between user input and the SD card, transitioning to reading mode if a file is found or showing an error if not.
void openWikiArticle(String name) {
  name.trim(); 
  name.toLowerCase(); 
  
  String folderPath = getPathFromTitle(name);
  String fileName = cleanFilename(name) + ".txt";
  currentFilename = folderPath + fileName;

  Serial.print("Opening: ");
  Serial.println(currentFilename);

  if (SD.exists(currentFilename)) {
    filePosition = 0; 
    endOfFile = false;
    currentState = READING;
    displayNextPage(); 
  } 
  else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.drawCentreString("FILE NOT FOUND", 160, 100, 2);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString(currentFilename, 160, 140, 1);
    
    delay(2000);
    
    tft.fillScreen(TFT_BLACK);
    drawKeyboard();
    updateSearchBar();
    currentState = KEYBOARD; 
  }
}

// Translates X/Y touch coordinates into specific key presses, updating the search query or triggering the search load state.
void handleKeyboard() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    resetTimer();
    if (y >= keyboardTopY) {
      int col = (x - 5) / (keyW + keyGap);
      int row = (y - keyboardTopY) / (keyH + keyGap);
      
      if (row >= 0 && row < 4 && col >= 0 && col < 10) {
        char pressedKey = keys[row][col];

        if (pressedKey == '!') { 
          currentState = LOADING; 
        } else if (pressedKey == '<') {
          if (searchQuery.length() > 0) searchQuery.remove(searchQuery.length() - 1);
          updateSearchBar();
        } else {
          searchQuery += pressedKey;
          updateSearchBar();
        }
        delay(200); 
      }
    }
  }
}

// Gives the user visual feedback that the device is processing their request, preventing them from thinking the system froze.
void handleLoading() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.drawCentreString("SEARCHING SD CARD...", 160, 110, 2);
  delay(100); 
  openWikiArticle(searchQuery); 
}

// Detects presses on the 'NEXT' and 'EXIT' buttons, allowing the user to navigate forward through the document or return to the search bar.
void handleReading() {
  uint16_t x, y;
  
  if (tft.getTouch(&x, &y)) {
    resetTimer();
    if (y > 210) {
      if (x < 100) { 
        searchQuery = ""; 
        tft.fillScreen(TFT_BLACK);
        drawKeyboard();
        updateSearchBar();
        currentState = KEYBOARD;
        delay(300); 
      }
      else if (x > 220 && !endOfFile) {  
        displayNextPage(); 
        delay(300); 
      }
    }
  }
}


// ==========================================
//   9. MAIN SETUP & LOOP
// ==========================================

// Initializes hardware, mounts the SD card, and routes the UI back to where the user left off if waking from sleep.
void setup() {
  Serial.begin(115200);

  pinMode(T_IRQ, INPUT_PULLUP);   
  pinMode(TFT_LED_PIN, OUTPUT);
  digitalWrite(TFT_LED_PIN, HIGH);
  pinMode(TFT_CS_PIN, OUTPUT);
  digitalWrite(TFT_CS_PIN, HIGH);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_EXT0) {
      Serial.println("Woke up from Touch!");
  }

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("FAILED!");
    tft.setTextColor(TFT_RED);
    tft.drawString("SD Init Failed!", 10, 40, 2);
    while (1);
  }

  searchQuery = String(savedSearchQuery);
  currentFilename = String(savedFilename);
  filePosition = savedFilePosition;

  if (currentState == READING && currentFilename != "") {
    displayNextPage();
  } else {
    tft.fillScreen(TFT_BLACK);
    drawKeyboard();
    updateSearchBar();
    currentState = KEYBOARD;
  }

  resetTimer();
}

// Continuously listens for input based on the current screen state and triggers deep sleep if idle too long.
void loop() {
  switch (currentState) {
    case KEYBOARD: handleKeyboard(); break;
    case LOADING:  handleLoading(); break;
    case READING:  handleReading(); break;
  }

  if (digitalRead(T_IRQ) == HIGH && millis() - lastActivityTime > SLEEP_TIMEOUT) {
    goToSleep();
  }
}