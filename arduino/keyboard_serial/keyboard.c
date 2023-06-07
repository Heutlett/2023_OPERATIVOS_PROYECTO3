#include <stdio.h>
#include <string.h>

void print_keyboard_matrix(char matrix[][3], int current_pos[2], int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (i == current_pos[0] && j == current_pos[1]) {
                printf("%c*,", matrix[i][j]);
            } else {
                printf("%c,", matrix[i][j]);
            }
        }
        printf("\n");
    }
}

void move_right(int current_pos[2], int max_cols) {
    printf("move_right\n");
    if (current_pos[1] < max_cols - 1) {
        current_pos[1]++;
    }
}

void move_left(int current_pos[2]) {
    printf("move_left\n");
    if (current_pos[1] > 0) {
        current_pos[1]--;
    }
}

void move_down(int current_pos[2], int max_rows) {
    printf("move_down\n");
    if (current_pos[0] < max_rows - 1) {
        current_pos[0]++;
    }
}

void move_up(int current_pos[2]) {
    printf("move_up\n");
    if (current_pos[0] > 0) {
        current_pos[0]--;
    }
}

int find_number_position(char matrix[][3], int rows, int cols, char number, int* target_pos) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (matrix[i][j] == number) {
                target_pos[0] = i;
                target_pos[1] = j;
                return 1;
            }
        }
    }
    return 0;
}

void get_movement(int current_pos[2], int target_pos[2], char* movements, int max_movements) {
    int dx = target_pos[1] - current_pos[1];
    int dy = target_pos[0] - current_pos[0];
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

int main() {
    char keyboard_matrix[][3] = {{'1', '2', '3'},
                                 {'4', '5', '6'},
                                 {'7', '8', '9'},
                                 {'d', '0', 'r'}};
    int current_position[2] = {0, 0};  // Initial position
    int rows = sizeof(keyboard_matrix) / sizeof(keyboard_matrix[0]);
    int cols = sizeof(keyboard_matrix[0]) / sizeof(char);

    print_keyboard_matrix(keyboard_matrix, current_position, rows, cols);

    while (1) {
        char target_number[2];
        printf("Enter target number: ");
        scanf("%s", target_number);

        if (strcmp(target_number, "exit") == 0) {
            break;
        }

        int target_position[2];
        if (!find_number_position(keyboard_matrix, rows, cols, target_number[0], target_position)) {
            printf("Target number not found. Please try again.\n");
            continue;
        }

        char movements[10];
        get_movement(current_position, target_position, movements, sizeof(movements) / sizeof(char));
        
        for (int i = 0; movements[i] != '\0'; i++) {
            if (movements[i] == 'r') {
                move_right(current_position, cols);
            } else if (movements[i] == 'l') {
                move_left(current_position);
            } else if (movements[i] == 'd') {
                move_down(current_position, rows);
            } else if (movements[i] == 'u') {
                move_up(current_position);
            }

            
        }

        print_keyboard_matrix(keyboard_matrix, current_position, rows, cols);
    }

    return 0;
}
