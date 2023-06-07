#include <Servo.h>

const int ROWS = 4;
const int COLS = 3;
char keyboard_matrix[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { 'd', '0', 'r' }
};
int current_position[2] = { 1, 1 };  // Posición inicial

Servo myservo_UD;
Servo myservo_LF;
Servo myservo_FB;

int pos_UD = 90;
int pos_LF = 90;
int pos_FB = 110;

int pos_UD_height_max = 100;
int pos_UD_height_touch;

int current_size = 'a';

int mov_LF_size = 12;
int mov_FB_size = 12;
int mov_size_offset = 0;

int mov_speed = 500;

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
  pos_LF = pos_LF - mov_LF_size;
  myservo_LF.write(pos_LF);
  delay(mov_speed);
  if (current_position[1] < COLS - 1) {
    current_position[1]++;
  }
}

void move_left() {
  Serial.println("move_left");
  pos_LF = pos_LF + mov_LF_size;
  myservo_LF.write(pos_LF);
  delay(mov_speed);
  if (current_position[1] > 0) {
    current_position[1]--;
  }
}

void move_down() {
  Serial.println("move_down");

  if(current_size == 'a'){
    mov_size_offset = 0;
  }

  if(current_size == 'b'){
    mov_size_offset = -2;
  }

  pos_FB = pos_FB - (mov_FB_size + mov_size_offset);

  myservo_FB.write(pos_FB);
  delay(mov_speed);
  if (current_position[0] < ROWS - 1) {
    current_position[0]++;
  }
}

void move_up() {
  Serial.println("move_up");
  pos_FB = pos_FB + mov_FB_size;
  myservo_FB.write(pos_FB);
  delay(mov_speed);
  if (current_position[0] > 0) {
    current_position[0]--;
  }
}

void press_screen() {

  Serial.print("Current position: ");
  Serial.print(current_position[0]);
  Serial.print(",");
  Serial.println(current_position[1]);

  if(current_size == 'a'){
    mov_size_offset = 0;
  }
  if(current_size == 'b'){
    mov_size_offset = -2;
  }

  if (current_position[0] == 0) {
    pos_UD_height_touch = 78 + mov_size_offset;
  } else if (current_position[0] == 1) {
    pos_UD_height_touch = 64 + mov_size_offset;
  } else if (current_position[0] == 2) {
    pos_UD_height_touch = 53 + mov_size_offset;
  } else if (current_position[0] == 3) {
    pos_UD_height_touch = 40 + mov_size_offset;
  }

  for (pos_UD = pos_UD_height_max; pos_UD <= pos_UD_height_max; pos_UD++) {
    myservo_UD.write(pos_UD);
    delay(6);
  }

  for (pos_UD = pos_UD_height_max; pos_UD >= pos_UD_height_touch; pos_UD--) {
    myservo_UD.write(pos_UD);
    delay(6);
  }
  for (pos_UD = pos_UD_height_touch; pos_UD <= pos_UD_height_max; pos_UD++) {
    myservo_UD.write(pos_UD);
    delay(6);
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
  Serial.println("------------------------- STARTED -------------------------");
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
    mov_size_offset = 0;

    if (target_number.equals("a\n")) {
      Serial.print("Size changed to: ");
      Serial.println(target_number);
      target_number.trim();
      current_size = 'a';
      mov_LF_size = 12;
      mov_FB_size = 12;
    } else if (target_number.equals("b\n")) {
      Serial.print("Size changed to: ");
      Serial.println(target_number);
      target_number.trim();
      current_size = 'b';
      mov_LF_size = 7;
      mov_FB_size = 13; // 14
    } else if (target_number.equals("c\n")) {
      Serial.print("Size changed to: ");
      Serial.println(target_number);
      target_number.trim();
      current_size = 'c';
      mov_LF_size = 8;
      mov_FB_size = 12;
    } else {

      Serial.print("Number to be pressed: ");
      Serial.println(target_number);
      target_number.trim();

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
}