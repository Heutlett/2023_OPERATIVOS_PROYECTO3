#include <Servo.h>

const int ROWS = 4;
const int COLS = 3;
char keyboard_matrix[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { 'd', '0', 'r' }
};
int current_position[2] = { 3, 1 };  // Posición inicial

Servo myservo_UD;
Servo myservo_LR;
Servo myservo_FB;

int start_pos_UD = 110;
int start_pos_LR = 90;
int start_pos_FB = 90;

int previous_UD;

int pos_UD = start_pos_UD;
int pos_LR = start_pos_LR;
int pos_FB = start_pos_FB;

int pos_UD_height_max = 110;
int pos_UD_height_touch;

int current_size = 'b';

int mov_LR_size;
int mov_FB_size;
int mov_size_offset = 0;

int delay_next_mov = 0;
int delay_go_start = 0;
int mov_speed_UD = 0;
int mov_speed_LR_FB = 0;

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
  int target_pos = pos_LR - mov_LR_size;
  for (int pos = pos_LR; pos >= target_pos; pos--) {
    myservo_LR.write(pos);
    delay(mov_speed_LR_FB);
  }
  pos_LR = target_pos;
  
  if (current_position[1] < COLS - 1) {
    current_position[1]++;
  }
}

void move_left() {
  Serial.println("move_left");
  int target_pos = pos_LR + mov_LR_size;
  for (int pos = pos_LR; pos <= target_pos; pos++) {
    myservo_LR.write(pos);
    delay(mov_speed_LR_FB); 
  }
  pos_LR = target_pos;
  
  if (current_position[1] > 0) {
    current_position[1]--;
  }
}

void move_down() {
  Serial.println("move_down");
  int target_pos = pos_FB - mov_FB_size;
  for (int pos = pos_FB; pos >= target_pos; pos--) {
    myservo_FB.write(pos);
    delay(mov_speed_LR_FB); 
  }
  pos_FB = target_pos;
  
  if (current_position[0] < ROWS - 1) {
    current_position[0]++;
  }
}

void move_up() {
  Serial.println("move_up");
  int target_pos = pos_FB + mov_FB_size;
  for (int pos = pos_FB; pos <= target_pos; pos++) {
    myservo_FB.write(pos);
    delay(mov_speed_LR_FB); 
  }
  pos_FB = target_pos;
  
  if (current_position[0] > 0) {
    current_position[0]--;
  }
}


void go_start_pos(){
  pos_UD = start_pos_UD;
  pos_LR = start_pos_LR;
  pos_FB = start_pos_FB;

  current_position[0] = 3;
  current_position[1] = 1; 

  myservo_LR.write(pos_LR);
  delay(delay_go_start);
  myservo_FB.write(pos_FB);
  delay(delay_go_start);
  myservo_UD.write(pos_UD);
  delay(delay_go_start);
}

void press_screen() {
  //delay(500);

  Serial.print("Current position: ");
  Serial.print(current_position[0]);
  Serial.print(",");
  Serial.println(current_position[1]);

  if (current_size == 'b') {
    mov_size_offset = 0;
  }
  if (current_size == 'm') {
    mov_size_offset = 0;
  }

  if (current_size == 's') {
    mov_size_offset = 0;
  }

  if (current_position[0] == 0) {
    pos_UD_height_touch = 70 + mov_size_offset;
  } else if (current_position[0] == 1) {
    pos_UD_height_touch = 64 + mov_size_offset;
  } else if (current_position[0] == 2) {
    pos_UD_height_touch = 43 + mov_size_offset;
  } else if (current_position[0] == 3) {
    pos_UD_height_touch = 20 + mov_size_offset;
  }
  //myservo_UD.write(pos_UD_height_touch);
  //myservo_UD.write(previous_UD);

  myservo_UD.write(pos_UD_height_touch);

  // for (int pos = pos_UD; pos >= pos_UD_height_touch; pos--) {
    
  //   delay(mov_speed_UD);
  // }


  // for (pos_UD = pos_UD_height_max; pos_UD <= pos_UD_height_max; pos_UD++) {
  //   myservo_UD.write(pos_UD);
  //   delay(mov_speed_UD);
  // }
  // for (pos_UD = pos_UD_height_max; pos_UD >= pos_UD_height_touch; pos_UD--) {
  //   myservo_UD.write(pos_UD);
  //   delay(mov_speed_UD);
  // }
  // for (pos_UD = pos_UD_height_touch; pos_UD <= pos_UD_height_max; pos_UD++) {
  //   myservo_UD.write(pos_UD);
  //   delay(mov_speed_UD);
  // }
  
  //go_start_pos();
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

  myservo_LR.attach(9);
  myservo_LR.write(start_pos_LR);

  delay(100);

  myservo_FB.attach(10);
  myservo_FB.write(start_pos_FB);

  delay(100);

  myservo_UD.attach(8);
  myservo_UD.write(start_pos_UD);

  current_size = 'b';
}

void loop() {
  if (Serial.available()) {
    String target_number = Serial.readStringUntil(' ');
    mov_size_offset = 0;

    if (target_number.equals("b\n")) {
      Serial.print("Size changed to: ");
      Serial.println(target_number);
      target_number.trim();
      current_size = 'b';
    } else if (target_number.equals("m\n")) {
      Serial.print("Size changed to: ");
      Serial.println(target_number);
      target_number.trim();
      current_size = 'm';
    } else if (target_number.equals("s\n")) {
      Serial.print("Size changed to: ");
      Serial.println(target_number);
      target_number.trim();
      current_size = 's';
    } else {

      if (current_size == 'b') {
        mov_LR_size = 12;
        mov_FB_size = 20;
      } else if (current_size == 'm') {
        mov_LR_size = 7;
        mov_FB_size = 13;  // 14
      } else if (current_size == 's') {
        mov_LR_size = 12;
        mov_FB_size = 18;
      }

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
        delay(delay_next_mov);
      }

      press_screen();

      print_keyboard_matrix();

      go_start_pos();
    }
  }
}
