#include <Servo.h>

const int ROWS = 4;
const int COLS = 3;
char keyboard_matrix[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'d', '0', 'r'}
};
int current_position[2] = {1, 1};  // Posición inicial

Servo myservo_UD;
Servo myservo_LF;
Servo myservo_FB;

int pos_UD = 90;
int pos_LF = 90;
int pos_FB = 110;

int pos_UD_height_max = 100;
int pos_UD_height_touch;

int mov_size = 12;

void print_keyboard_matrix() {
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      if (i == current_position[0] && j == current_position[1]) {
        Serial.print(keyboard_matrix[i][j]);
        Serial.print("*,");
      } else {
        Serial.print(keyboard_matrix[i][j]);
        Serial.print(",");
      }
    }
    Serial.println();
  }
}

void move_right() {
  Serial.println("move_right");
  pos_LF = pos_LF - mov_size;
  myservo_LF.write(pos_LF);
  delay(500);
  if (current_position[1] < COLS - 1) {
    current_position[1]++;
  }
}

void move_left() {
  Serial.println("move_left");
  pos_LF = pos_LF + mov_size;
  myservo_LF.write(pos_LF);
  delay(500);
  if (current_position[1] > 0) {
    current_position[1]--;
  }
}

void move_down() {
  Serial.println("move_down");
  pos_FB = pos_FB - mov_size;
  myservo_FB.write(pos_FB);
  delay(500);
  if (current_position[0] < ROWS - 1) {
    current_position[0]++;
  }
}

void move_up() {
  Serial.println("move_up");
  pos_FB = pos_FB + mov_size;
  myservo_FB.write(pos_FB);
  delay(500);
  if (current_position[0] > 0) {
    current_position[0]--;
  }
}

void press_screen() {

  Serial.print("Current position: ");
  Serial.print(current_position[0]);
  Serial.print(",");
  Serial.println(current_position[1]);

  if(current_position[0] == 0){
    pos_UD_height_touch = 80;
  }
  else if(current_position[0] == 1){
    pos_UD_height_touch = 68;
  }
  else if(current_position[0] == 2){
    pos_UD_height_touch = 53;
  }
  else if(current_position[0] == 3){
    pos_UD_height_touch = 40;
  }

  for (pos_UD = pos_UD_height_max; pos_UD <= pos_UD_height_max; pos_UD++) {
    myservo_UD.write(pos_UD);
    delay(4);
  }

  for (pos_UD = pos_UD_height_max; pos_UD >= pos_UD_height_touch; pos_UD--) {
    myservo_UD.write(pos_UD);
    delay(4);
  }
  for (pos_UD = pos_UD_height_touch; pos_UD <= pos_UD_height_max; pos_UD++) {
    myservo_UD.write(pos_UD);
    delay(4);
  }
}

int find_number_position(char number, int* target_pos) {
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      if (keyboard_matrix[i][j] == number) {
        target_pos[0] = i;
        target_pos[1] = j;
        return 1;
      }
    }
  }
  return 0;
}

void get_movement(int target_pos[2], char* movements, int max_movements) {
  int dx = target_pos[1] - current_position[1];
  int dy = target_pos[0] - current_position[0];
  int movement_count = 0;

  if (dx > 0) {
    for (int i = 0; i < dx; i++) {
      movements[movement_count++] = 'r';
    }
  } else if (dx < 0) {
    for (int i = 0; i < -dx; i++) {
      movements[movement_count++] = 'l';
    }
  }

  if (dy > 0) {
    for (int i = 0; i < dy; i++) {
      movements[movement_count++] = 'd';
    }
  } else if (dy < 0) {
    for (int i = 0; i < -dy; i++) {
      movements[movement_count++] = 'u';
    }
  }

  if (movement_count < max_movements) {
    movements[movement_count] = '\0';
  }
}



void setup() {
  Serial.begin(9600);
  print_keyboard_matrix();

  myservo_UD.attach(8);
  myservo_UD.write(pos_UD);

  myservo_LF.attach(9);
  myservo_LF.write(pos_LF);

  myservo_FB.attach(10);
  myservo_FB.write(pos_FB);
}

void loop() {
  if (Serial.available()) {
    String target_number = Serial.readStringUntil(' ');
    Serial.print("Number to be pressed: ");
    Serial.println(target_number);
    target_number.trim();

    if (target_number.equals("exit")) {
      return;
    }

    int target_position[2];
    if (!find_number_position(target_number[0], target_position)) {
      Serial.println("Target number not found. Please try again.");
      return;
    }

    char movements[10];
    get_movement(target_position, movements, sizeof(movements) / sizeof(char));

    for (int i = 0; movements[i] != '\0'; i++) {
      if (movements[i] == 'r') {
        move_right();
      } else if (movements[i] == 'l') {
        move_left();
      } else if (movements[i] == 'd') {
        move_down();
      } else if (movements[i] == 'u') {
        move_up();
      }
    }

    press_screen();

    print_keyboard_matrix();
  }
}
