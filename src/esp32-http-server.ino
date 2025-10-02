#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include "FS.h"
#include "LittleFS.h"
#include <HTTPClient.h>  // For HTTP requests
#include <Update.h>

struct Song; // Forward declaration

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6

#define AP_SSID "bells"  // Set your Hotspot name (SSID)
#define AP_PASS "88887777"       // Set your password (8+ characters)

#define IS_EMULATOR 0

WebServer server(80);

struct Output {
  int id;
  int pin;
  bool isOn;

  void setState(bool state) {
    if (state) {
      if (!isOn) {
        isOn = true;
        digitalWrite(pin, LOW);
      }
    } else {
      if (isOn) {
        isOn = false;
        digitalWrite(pin, HIGH);
      }
    }
  }
};

Output EMPTY_OUTPUT = { -1, -1 };

Output outputs[] = {
  {1, 21, false},
  {2, 19, false},
  {3, 18, false},
  {4, 5, false},
};

Output& getOutputById(int outId) {
  for (int i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
    if (outputs[i].id == outId) {
      return outputs[i];
    }
  }

  return EMPTY_OUTPUT;
}

struct Tact {
  bool values[4];
  int delay;

  Tact(const int (&vals)[4], int d) : delay(d) {
    for (int i = 0; i < 4; ++i) {
      values[i] = vals[i];
    }
  }

  Tact() : delay(0) {
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;
    values[3] = 0;
  }
};

struct Node {
  Tact tact;
  Node *next;

  Node(Tact value) : tact(value), next(nullptr) {}
  Node(std::nullptr_t) : next(nullptr) {}

  Node* push(Node* value) {
      Node* temp = this;
      temp->next = value;
      return this->next;
  }

  Node* nextNode() {
      return next;
  }

  void play() {
    if (tact.delay == 0) {
      return;
    }

    bool *values = tact.values;
    for (int i = 0; i < 4; i++) {
      getOutputById(i + 1).setState(tact.values[i]);
    }
  };
};

struct Song {
  int id;
  Node* head;
  int time;
  int endTime = 0;

  Song(int id) : id(id), head(nullptr), time(0) {}

  void setDuration(int duration) {
    if (duration > 0) {
     endTime = millis() + (duration * 1000); // seconds to milliseconds
    }
  }

  void play() {
    // Serial.println("song play: " + String(id) + " " + String(endTime) + " " + String(millis()));
    if (endTime > 0 && millis() > endTime) {
      Serial.println("song end: " + String(id));
      playSong(id, false); // rethink this
      return;
    }
  
    if(id == -1 || millis() < time) {
      return;
    }

    Node* current = head;
    current->play();
    head = current->nextNode(); 
    time = millis() + current->tact.delay;
  }

  ~Song() {
    if (!head) return;

    Node* current = head->next;
    Node* nextNode;

    while (current != head) {
        nextNode = current->next;
        delete current;
        current = nextNode;
    }

    delete head;
    head = nullptr;
  }
};

struct Input {
  int pin;
  std::function<void(Input&)> func;
  bool isOn;

  int check() {
    bool newValue = digitalRead(pin) == LOW;
    
    int result = 0;
    if(newValue != isOn) {
      if (newValue == 0) {
        result = 2;
      } else {
        result = 1;
      }
      
      isOn = newValue;
    }
    
    return result;
  }

  void execute() {
    if(check() > 0) {
      func(*this);
    }
  }
};

Input inputs[] = {
  // buttons(outside)
  {17, [](Input& x) { playSongWithDuration(8, x.isOn); }},
  {4, [](Input& x) { playSongWithDuration(9, x.isOn); }},
  {16, [](Input& x) { playSongWithDuration(10, x.isOn); }},

  // butons(bells)
  {13, [](Input& x) { getOutputById(1).setState(x.isOn); }},
  {12, [](Input& x) { getOutputById(2).setState(x.isOn); }},
  {14, [](Input& x) { getOutputById(3).setState(x.isOn); }},
  {27, [](Input& x) { getOutputById(4).setState(x.isOn); }},
  
  {26, [](Input& x) { playSong(1, x.isOn); }},
  {25, [](Input& x) { playSong(2, x.isOn); }},
  {33, [](Input& x) { playSong(3, x.isOn); }},
};

void controlOutput(bool state) {
  String led = server.pathArg(0);  
  int id = led.toInt();
  Output& out = getOutputById(id);
  
  if (out.pin != -1) {
    out.setState(state);
    String status = state ? "ON" : "OFF";
    server.send(200, "text/html", "Out " + led + " turned " + status + ".");
  } else {
    server.send(400, "text/html", "Error: LED not found.");
  }
}

Song* parseCSVString(const String& csv, int id = 1) {
  Song* song = new Song(id);
  Node* prevNode = nullptr;
  int startIdx = 0;

  while (startIdx < csv.length()) {
    int endIdx = csv.indexOf('\n', startIdx);
    if (endIdx == -1) {
      endIdx = csv.length();
    }

    String line = csv.substring(startIdx, endIdx);
    startIdx = endIdx + 1;

    int values[4];
    int delay;
    int commaIndex = 0;
            

      if (line.startsWith("-1,")) {
        int commaPos = line.indexOf(',', 3);
        String duration = (commaPos == -1) ? line.substring(3) : line.substring(3, commaPos);
        song->setDuration(duration.toInt());
        Serial.println("Parsed duration: " + duration);
        continue;
      }
    for (int i = 0; i < 4; ++i) {
      commaIndex = line.indexOf(',', commaIndex);
      if (commaIndex == -1) {
        break;
      }
      values[i] = line.substring(0, commaIndex).toInt();
      line = line.substring(commaIndex + 1);
    }
    delay = line.toInt();

    Tact tact(values, delay);
    Node* newNode = new Node(tact);

    if (prevNode == nullptr) {
      song->head = newNode;
    } else {
      prevNode->push(newNode);
    }

    prevNode = newNode;
  }

  prevNode->push(song->head);

  return song;
};

Song* loadSongFromFile(const String& melodyFileName, int id = 1) {
  Serial.println("loadSongFromFile: " + melodyFileName);

  File file = LittleFS.open(melodyFileName, "r");
  if (!file) {
      Serial.println("Failed to open file for reading");
      return nullptr;
  }

  Song* song = new Song(id);
  Node* prevNode = nullptr;
  
  String line;
  while (file.available()) {
      line = file.readStringUntil('\n');  // Read a single line
      line.trim(); // Remove leading/trailing spaces
      if (line.length() == 0) {
        continue; // Skip empty lines
      }

      if (line.startsWith("-1,")) {
        int commaPos = line.indexOf(',', 3);
        String duration = (commaPos == -1) ? line.substring(3) : line.substring(3, commaPos);
        song->setDuration(duration.toInt());
        Serial.println("Parsed duration: " + duration);
        continue;
      } else if (line.startsWith("-2,")) {
          // TODO: Handle -2 case
      }
      
      int values[4] = {0};  // Default values
      int delay = 0;
      
      int index = 0, lastPos = 0, fieldCount = 0;
      while ((index = line.indexOf(',', lastPos)) != -1 && fieldCount < 4) {
          String field = line.substring(lastPos, index);
          values[fieldCount++] = field.length() > 0 ? field.toInt() : 0;  // Convert or set 0 for empty
          lastPos = index + 1;
      }
      
      // Last value (delay) after the last comma
      String field = line.substring(lastPos);
      delay = field.length() > 0 ? field.toInt() : 0;

      // Create a new node
      Tact tact(values, delay);
      Node* newNode = new Node(tact);

      // Link nodes
      if (!prevNode) {
          song->head = newNode;
      } else {
          prevNode->push(newNode);
      }

      prevNode = newNode;
  }

  file.close();

  // Close the circular list
  if (prevNode && song->head) {
      prevNode->push(song->head);
  }

  return song;
};

Song* song;

Song* getSong1(int id) {
  String csvData = "-1,5000\n1,0,1,0,200\n1,1,1,1,100\n1,0,0,0,150\n0,1,0,0,300\n0,0,1,0,100\n0,0,0,1,50\n";
  return parseCSVString(csvData, id);
};

String melodyFileName(String id) {
  return "/melody_" + id + ".csv";
}

void playSongWithDuration(int id, bool isPlay) {
  Serial.println("playSongWithDuration start : " + String(id) + " " + String(isPlay));
  if(!isPlay) {
    return;
  }
  
  int isTheSameSong = song && id == song->id;
  Serial.println("playSongWithDuration: " + String(id) + " " + String(song && song->id) + " " + String(isTheSameSong));
  playSong(id, !isTheSameSong);
}

void playSong(int id, bool isPlay) {
  Serial.println("playSong: " + String(id) + " " + String(isPlay));
  if (!isPlay || id == 0 || (song && id != song->id)) {
    if (song) {
      delete song;
      song = nullptr;  
    }
    for (int i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
      outputs[i].setState(false);
    }
    if (id == 0 || !isPlay) {
      return;
    }
  }

  if (IS_EMULATOR) {
    song = getSong1(id);
    return;
  }

  song = loadSongFromFile(melodyFileName(String(id)), id);
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  static File file;

  if (upload.status == UPLOAD_FILE_START) {
    String filename = melodyFileName(server.pathArg(0));
    file = LittleFS.open(filename, "w");
    if (!file) {
      server.send(500, "text/plain", "Failed to create file");
      return;
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (file) {
      file.write(upload.buf, upload.currentSize);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (file) {
      file.close();
      server.send(200, "text/plain", "File uploaded successfully.");
    }
  } 
}

void handleFileDownload() {
  String filename = server.pathArg(0);
  if (filename.isEmpty()) {
    server.send(400, "text/plain", "Bad Request: Missing filename.");
    return;
  }

  filename = melodyFileName(filename);

  if (!LittleFS.exists(filename)) {
    server.send(404, "text/plain", "File not found.");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open file.");
    return;
  }

  server.streamFile(file, "text/plain");
  file.close();
}

void sendHtml() {
//   String response = R"( PASTE indexhtml for wowki simulator)";
  
//   server.send(200, "text/html", response);

  File file = LittleFS.open("/index.html", "r");
  server.streamFile(file, "text/html");
  file.close();
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("Starting...");
  for (int i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
    pinMode(outputs[i].pin, OUTPUT);
    digitalWrite(outputs[i].pin, HIGH); 
  }

  for (int i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
    pinMode(inputs[i].pin, INPUT_PULLUP);
  }

  if (IS_EMULATOR) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }
  } else {
    WiFi.softAP(AP_SSID, AP_PASS);

    if (!LittleFS.begin(true)) {
      Serial.println("SPIFFS initialization failed!");
      return;
    }
  }

  server.on("/", HTTP_GET, sendHtml);
  server.on(UriBraces("/output/{}"), HTTP_POST, []() {controlOutput(true);});
  server.on(UriBraces("/output/{}"), HTTP_DELETE, []() {controlOutput(false);});
  server.on(UriBraces("/melody/{}"), HTTP_POST, []() {
    String led = server.pathArg(0);  
    int id = led.toInt();
    playSong(id, true);
    server.send(200, "text/html", "Melody " + String(id));
  });

  server.on(UriBraces("/file/{}"), HTTP_GET, handleFileDownload);
  server.on(UriBraces("/file/{}"), HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  }, handleFileUpload);

  server.begin();
  // http://localhost:8180 use this link to access the server for emulator;
}

void loop(void) {
  server.handleClient();

  for (int i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
    Input& input = inputs[i];
    input.execute();
  }

  if (song){
    song->play();
  }

  delay(10);
}
