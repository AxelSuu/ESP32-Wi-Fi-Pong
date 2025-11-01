#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <SPIFFS.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>
#include <SPI.h>

// ------------------- OLED SETUP -------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 96

// ---------------- Pin definitions -----------------
#define OLED_CS    6
#define OLED_DC    5
#define OLED_RST   4
#define OLED_MOSI  11
#define OLED_SCLK  12

// ------------------- Using SPI  --------------------
Adafruit_SSD1327 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI, OLED_SCLK,
                         OLED_DC, OLED_RST, OLED_CS);

// ------------------- WIFI + SERVER -----------------
const char* ssid = "ESP32-Pong";
const char* password = "12345678";

WebServer server(80);
WebSocketsServer webSocket(81);

// ------------------- GAME STATE --------------------
enum GameState {
  WAITING_FOR_START,
  PLAYING,
  GAME_OVER
};

GameState currentState = WAITING_FOR_START;

// ------------------- GAME OBJECTS -------------------
struct Paddle { int x, y, w, h; };
struct Ball { int x, y, dx, dy, r; };

Paddle player = {5, 30, 4, 20};
Paddle enemy  = {SCREEN_WIDTH - 9, 30, 4, 20};
Ball ball     = {SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2, 1, 3};


int playerScore = 0;
int enemyScore = 0;
int aiReactionDelay = 0;
int aiTargetOffset = 0;
unsigned long lastAiUpdate = 0;
unsigned long gameStartTime = 0;
unsigned long lastSpeedIncrease = 0;
const unsigned long SPEED_INCREASE_INTERVAL = 10000; // 10 seconds in milliseconds

// ------------------- GAME FUNCTIONS -------------------
void resetBall() {
  ball.x = SCREEN_WIDTH / 2;
  ball.y = SCREEN_HEIGHT / 2;
  ball.dx = 4;
  ball.dy = (random(2) == 0) ? 1 : -1;
}

void startGame() {
  currentState = PLAYING;
  playerScore = 0;
  enemyScore = 0;
  resetBall();
  
  // Reset paddle positions
  player.y = 30;
  enemy.y = 30;

  // Initialize speed increase timing
  gameStartTime = millis();
  lastSpeedIncrease = gameStartTime;
}

void drawStartScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);
  
  // Title
  display.setCursor(25, 10);
  display.println("ESP32 PONG");
  
  // Instructions
  display.setCursor(10, 30);
  display.println("Connect to WiFi:");
  display.setCursor(15, 42);
  display.println("ESP32-Pong");
  
  display.setCursor(10, 58);
  display.println("Open browser and");
  display.setCursor(10, 70);
  display.println("click START GAME");
  
  // System info
  display.setCursor(5, 85);
  display.printf("IP: %s", WiFi.softAPIP().toString().c_str());
  
  display.display();
}

void updateGame() {
  if (currentState != PLAYING) return;

  // Check for ball speed increase every 10 seconds
  unsigned long currentTime = millis();
  if (currentTime - lastSpeedIncrease >= SPEED_INCREASE_INTERVAL) {
    // Increase ball speed (preserve direction)
    if (ball.dx > 0) {
      ball.dx += 1;
    } else {
      ball.dx -= 1;
    }
    lastSpeedIncrease = currentTime;
    Serial.printf("Ball speed increased! New dx: %d\n", ball.dx);
  }
  
  ball.x += ball.dx;
  ball.y += ball.dy;

  // Ball collision with top/bottom walls
  if (ball.y - ball.r <= 0 || ball.y + ball.r >= SCREEN_HEIGHT) {
    ball.dy *= -1;
  }

  // Ball collision with player paddle
  if (ball.x - ball.r <= player.x + player.w &&
      ball.y >= player.y && ball.y <= player.y + player.h &&
      ball.dx < 0) {
    ball.dx *= -1;
  }

  // Ball collision with enemy paddle
  if (ball.x + ball.r >= enemy.x &&
      ball.y >= enemy.y && ball.y <= enemy.y + enemy.h &&
      ball.dx > 0) {
    ball.dx *= -1;
  }

  // Check for scoring (ball out of bounds)
  if (ball.x < 0) {
    enemyScore++;
    resetBall();
    // Reset speed increase timer when ball resets
    lastSpeedIncrease = millis();
  } else if (ball.x > SCREEN_WIDTH) {
    playerScore++;
    resetBall();
    // Reset speed increase timer when ball resets
    lastSpeedIncrease = millis();
  }

  // Imperfect enemy AI with reaction delay and targeting errors
  
  // Update AI decision every 250-500ms (random delay)
  if (currentTime - lastAiUpdate > aiReactionDelay) {
    lastAiUpdate = currentTime;
    aiReactionDelay = random(250, 500); // Random reaction time
    
    // Add random offset to target (overshooting/undershooting)
    aiTargetOffset = random(-10, 10); // Random offset between -8 and 8 pixels
  }
  
  // Only react if ball is moving toward enemy
  if (ball.dx > 0) {
    int enemyCenter = enemy.y + enemy.h / 2;
    int targetY = ball.y + aiTargetOffset; // Target with error
    
    // Move toward target with some randomness
    int moveSpeed = 2; // Variable speed 1-3 pixels
    
    if (targetY < enemyCenter - 3) {
      enemy.y -= moveSpeed; // Move up
    } else if (targetY > enemyCenter + 3) {
      enemy.y += moveSpeed; // Move down
    }
    
    // Occasionally make a mistake (10% chance)
    if (random(100) < 10) {
      enemy.y += random(-6, 6); // Random movement
    }
  }

  // Keep enemy paddle within screen bounds
  if (enemy.y < 0) enemy.y = 0;
  if (enemy.y + enemy.h > SCREEN_HEIGHT) enemy.y = SCREEN_HEIGHT - enemy.h;
  
  // Keep player paddle within screen bounds
  if (player.y < 0) player.y = 0;
  if (player.y + player.h > SCREEN_HEIGHT) player.y = SCREEN_HEIGHT - player.h;
}

void drawGame() {
  if (currentState == WAITING_FOR_START) {
    drawStartScreen();
    return;
  }
  
  display.clearDisplay();

  // Draw paddles
  display.fillRect(player.x, player.y, player.w, player.h, SSD1327_WHITE);
  display.fillRect(enemy.x, enemy.y, enemy.w, enemy.h, SSD1327_WHITE);

  // Draw ball
  display.fillCircle(ball.x, ball.y, ball.r, SSD1327_WHITE);

  // Draw scores
  display.setTextSize(1);
  display.setCursor(30, 5);
  display.print(playerScore);
  display.setCursor(90, 5);
  display.print(enemyScore);

  // Draw center line  
  for (int i = 0; i < SCREEN_HEIGHT; i += 4) {
    display.drawPixel(SCREEN_WIDTH / 2, i, SSD1327_WHITE);
  }

  display.display();
}

// ------------------- WEBSOCKET HANDLER -------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT && currentState == PLAYING) {
    String msg = String((char*)payload);
    if (msg.indexOf("left") >= 0) player.y -= 10;  // move up
    if (msg.indexOf("right") >= 0) player.y += 10; // move down
  }
}

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);

  // Init SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }

  // OLED init
  if (!display.begin(0x3D)) {  // Address param ignored in SPI mode
    Serial.println("SSD1327 allocation failed");
    for (;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1327_WHITE);
  display.display();

  // Wi-Fi AP
  WiFi.softAP(ssid, password);
  Serial.println("WiFi started: ESP32-Pong");

  // Serve index.html from SPIFFS
  server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      server.send(500, "text/plain", "index.html not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });
  
  // Handle start game request
  server.on("/start", HTTP_GET, []() {
    startGame();
    server.send(200, "text/plain", "Game Started!");
  });
  
  server.begin();

  // WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ------------------- LOOP -------------------
void loop() {
  server.handleClient();
  webSocket.loop();

  updateGame();
  drawGame();

  delay(30); // ~30 FPS
}