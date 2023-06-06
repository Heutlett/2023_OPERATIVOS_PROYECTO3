#include <Servo.h>

Servo myservo_up_down;
Servo myservo_left_right;
Servo myservo_front_back;

int pos_up_down;
int pos_left_right;

int pos_ini_up_down = 100;
int pos_media_up_down = 42;
int pos_final_up_down = 120;

int pos_ini_left_right = 90;
int pos_media_left_right = 60;
int pos_final_left_right = 100;

int delay_ = 10;

int start = 1;

int down_size[3][4] = { { 42, 53, 63, 75 }, { 47, 59, 70, 78 }, { 53, 63, 73, 78 } };

int first_row[3][3] = { { 105, 90, 75 }, { 98, 90, 82 }, { 98, 90, 82 } };
int other_rows[3][3] = { { 100, 90, 80 }, { 97, 90, 83 }, { 97, 90, 83 } };

int row_pos[3][4] = { { 90, 100, 110, 125 }, { 95, 105, 118, 130 }, { 100, 110, 120, 134 } };


void setup() {
  myservo_up_down.attach(8);
  myservo_up_down.write(90);

  myservo_left_right.attach(9);
  myservo_left_right.write(105);

  myservo_front_back.attach(10);
  myservo_front_back.write(90);

  delay(3000);
}

void press_screen() {
  for (pos_up_down = pos_ini_up_down; pos_up_down <= pos_final_up_down; pos_up_down++) {
    myservo_up_down.write(pos_up_down);
    delay(delay_);
  }

  for (pos_up_down = pos_final_up_down; pos_up_down >= pos_media_up_down; pos_up_down--) {
    myservo_up_down.write(pos_up_down);
    delay(delay_);
  }

  for (pos_up_down = pos_media_up_down; pos_up_down <= pos_ini_up_down; pos_up_down++) {
    myservo_up_down.write(pos_up_down);
    delay(delay_);
  }
}

void move_to(int row, int col, int size) {
  if (row == 0) {
    myservo_front_back.write(row_pos[size][row]);
    pos_media_up_down = down_size[size][row];
    myservo_left_right.write(first_row[size][col]);
  } else if (row == 1) {
    myservo_front_back.write(row_pos[size][row]);
    pos_media_up_down = down_size[size][row];
    myservo_left_right.write(other_rows[size][col]);
  } else if (row == 2) {
    myservo_front_back.write(row_pos[size][row]);
    pos_media_up_down = down_size[size][row];
    myservo_left_right.write(other_rows[size][col]);
  } else if (row == 3) {
    myservo_front_back.write(row_pos[size][row]);
    pos_media_up_down = down_size[size][row];
    myservo_left_right.write(other_rows[size][col]);
  }
}

void press_button(int n) {

  switch (n) {
    case 0: // Del:   size 100
      move_to(0, 0, 0);
      delay(1000);
      break;
    case 1: // 0:     size 100
      move_to(0, 1, 0);
      delay(1000);
      break;
    case 2: // Check: size 100
      move_to(0, 2, 0);
      delay(1000);
      break;
    case 3: // 7:     size 100
      move_to(1, 0, 0);
      delay(1000);
      break;
    case 4: // 4:     size 100
      move_to(2, 0, 0);
      delay(1000);
      break;
    case 5: // 5:     size 100
      move_to(2, 1, 0);
      delay(1000);
      break;
    case 6: // 6:     size 100
      move_to(2, 2, 0);
      delay(1000);
      break;
    case 7: // 1:     size 100
      move_to(3, 0, 0);
      delay(1000);
      break;
    case 8: // 2:     size 100
      move_to(3, 1, 0);
      delay(1000);
      break;
    case 9: // 3:     size 100
      move_to(3, 2, 0);
      delay(1000);
      break;
  }
  press_screen();
  delay(1000);
}

void loop() {

  if (start) {
    delay(1000);
    press_button(0);
    press_button(7);
    press_button(8);
    press_button(9);
    press_button(4);
    press_button(2);

    start = 0;
  }
}
