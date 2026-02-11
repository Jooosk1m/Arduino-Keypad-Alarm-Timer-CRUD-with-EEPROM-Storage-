#include <Keypad.h>
#include <TimeLib.h>
#include <EEPROM.h>

const int TRIGGER_PIN = 13; 
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {12, 11, 10, 9}; 
byte colPins[COLS] = {8, 7, 6, 5}; 
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

class AlarmTimer {
  private:
    String inputDigits = "";
    String meridiem = "";
    String savedAlarms[3] = {"Empty", "Empty", "Empty"};
    bool alarmActive[3] = {false, false, false}; 
    int currentAlarmIndex = 0; 
    int step = 0;

  public:
    void loadAllAlarms() {
      Serial.println(F("\n--- LOADING SAVED ALARMS ---"));
      for (int i = 0; i < 3; i++) {
        char t[5] = {0}; char m[3] = {0}; 
        int addr = i * 20; 
        EEPROM.get(addr, t);     
        EEPROM.get(addr + 10, m); 
        EEPROM.get(addr + 15, alarmActive[i]); 
        
        if (t[0] != (char)255 && t[0] != '\0') {
          String timeStr = String(t);
          savedAlarms[i] = timeStr.substring(0,2) + ":" + timeStr.substring(2) + " " + String(m);
        } else {
          savedAlarms[i] = "Empty";
          alarmActive[i] = false;
        }
      }
      printAlarms();
    }

    void handleInput(char key) {
      if (step == 0) {
        if (key == 'A') { step = 10; Serial.println(F("\n[SET] Select (1-3):")); return; }
        if (key == 'B') { step = 20; Serial.println(F("\n[DELETE] Select (1-3):")); return; }
        if (key == 'D') { step = 30; Serial.println(F("\n[TOGGLE] Select (1-3) to ON/OFF:")); return; }
      }

      if (step == 30) {
        if (key >= '1' && key <= '3') {
          int idx = key - '1';
          if (savedAlarms[idx] != "Empty") {
            alarmActive[idx] = !alarmActive[idx];
            EEPROM.put((idx * 20) + 15, alarmActive[idx]);
            Serial.print(F("Alarm ")); Serial.print(idx + 1);
            Serial.println(alarmActive[idx] ? F(" is now ONLINE") : F(" is now OFFLINE"));
          } else { Serial.println(F("Slot Empty.")); }
        }
        step = 0; printAlarms(); return;
      }

      if (step == 10) { if (key >= '1' && key <= '3') { startNewSet(key - '1'); return; } step = 0; return; }
      if (step == 20) { if (key >= '1' && key <= '3') { deleteAlarm(key - '1'); step = 0; return; } step = 0; return; }

      if (key == '#') { if (step == 1 && inputDigits.length() > 0) { inputDigits.remove(inputDigits.length()-1); displayProgress(); } return; }

      if (step == 1) {
        if (key >= '0' && key <= '9' && inputDigits.length() < 4) {
          inputDigits += key; displayProgress();
          if (inputDigits.length() == 2) {
            int h = inputDigits.toInt();
            if (h > 12 || h == 0) { Serial.println(F("\n[Error] 01-12!")); inputDigits = ""; }
          }
        }
        if (key == 'C' && inputDigits.length() == 4) {
          if (inputDigits.substring(2).toInt() > 59) Serial.println(F("\n[Error] Min > 59!"));
          else { step = 2; Serial.println(F("\n[STEP 2] 1:AM, 2:PM:")); }
        }
      }
      else if (step == 2) {
        if (key == '1') { meridiem = "AM"; Serial.println(F("Selected AM. Press 'C'.")); }
        if (key == '2') { meridiem = "PM"; Serial.println(F("Selected PM. Press 'C'.")); }
        if (key == 'C' && meridiem != "") { saveToEEPROM(); step = 0; printAlarms(); }
      }
    }

    void startNewSet(int index) {
      currentAlarmIndex = index; step = 1; inputDigits = ""; meridiem = "";
      Serial.print(F("\n--- EDITING ALARM ")); Serial.print(index + 1); Serial.println(F(" ---"));
    }

    void deleteAlarm(int index) {
      EEPROM.write(index * 20, '\0'); 
      alarmActive[index] = false;
      EEPROM.put((index * 20) + 15, false);
      savedAlarms[index] = "Empty";
      printAlarms();
    }

    void saveToEEPROM() {
      char tToSave[5]; char mToSave[3];
      inputDigits.toCharArray(tToSave, 5);
      meridiem.toCharArray(mToSave, 3);
      int baseAddr = currentAlarmIndex * 20;
      EEPROM.put(baseAddr, tToSave);
      EEPROM.put(baseAddr + 10, mToSave);
      alarmActive[currentAlarmIndex] = true;
      EEPROM.put(baseAddr + 15, true);
      savedAlarms[currentAlarmIndex] = inputDigits.substring(0,2) + ":" + inputDigits.substring(2) + " " + meridiem;
      Serial.println(F("\n[SAVED SUCCESS]"));
    }

    void checkAlarms() {
      static int lastSecond = -1;
      if (second() == lastSecond) return;
      lastSecond = second();

      String currentTime = (hourFormat12() < 10 ? "0" : "") + String(hourFormat12()) + ":" + (minute() < 10 ? "0" : "") + String(minute()) + " " + (isAM() ? "AM" : "PM");

      bool anyMatch = false;
      for (int i = 0; i < 3; i++) {
        if (alarmActive[i] && savedAlarms[i] == currentTime) {
          anyMatch = true;
          if (second() < 10) {
            digitalWrite(TRIGGER_PIN, HIGH);
            if (second() == 0) Serial.println(F("\n>>> TRIGGER ACTIVE!"));
          } else {
            digitalWrite(TRIGGER_PIN, LOW);
          }
        }
      }
      if (!anyMatch) digitalWrite(TRIGGER_PIN, LOW);
    }

    void displayProgress() {
      Serial.print("\rTime: "); 
      if (inputDigits.length() <= 2) Serial.print(inputDigits);
      else Serial.print(inputDigits.substring(0,2) + ":" + inputDigits.substring(2));
      Serial.print("    "); 
    }

    void printAlarms() {
      Serial.println(F("\n--- CURRENT ALARMS ---"));
      for (int i = 0; i < 3; i++) {
        Serial.print(i + 1); Serial.print(F(": ")); Serial.print(savedAlarms[i]);
        if (savedAlarms[i] != "Empty") {
          Serial.println(alarmActive[i] ? F(" [ONLINE]") : F(" [OFFLINE]"));
        } else Serial.println();
      }
      Serial.println(F("----------------------"));
    }
};

AlarmTimer myTimer;

void setup() {
  Serial.begin(9600);
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  
  setTime(1, 0, 0, 12, 2, 2026); 
  
  myTimer.loadAllAlarms();
  Serial.println(F("A:Set | B:Delete | D:Toggle ONLINE/OFFLINE"));
}

void loop() {
  char key = kpd.getKey();
  if (key) myTimer.handleInput(key);
  myTimer.checkAlarms();

  static int lastMin = -1;
  if (minute() != lastMin) {
    lastMin = minute();
    Serial.print(F("\n[CLOCK] "));
    Serial.print(hourFormat12());
    Serial.print(":");
    if (minute() < 10) Serial.print("0");
    Serial.print(minute());
    Serial.println(isAM() ? " AM" : " PM");
  }
}