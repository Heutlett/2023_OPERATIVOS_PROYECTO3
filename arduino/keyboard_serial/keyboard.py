def print_keyboard_matrix(matrix, current_pos):
    rows = len(matrix)
    cols = len(matrix[0])

    for i in range(rows):
        for j in range(cols):
            if (i, j) == current_pos:
                print(f"{matrix[i][j]}*,", end='')
            else:
                print(f"{matrix[i][j]},", end='')
        print()

def move_right(current_pos, max_cols):
    if current_pos[1] < max_cols - 1:
        return current_pos[0], current_pos[1] + 1
    else:
        return current_pos

def move_left(current_pos):
    if current_pos[1] > 0:
        return current_pos[0], current_pos[1] - 1
    else:
        return current_pos

def move_down(current_pos, max_rows):
    if current_pos[0] < max_rows - 1:
        return current_pos[0] + 1, current_pos[1]
    else:
        return current_pos

def move_up(current_pos):
    if current_pos[0] > 0:
        return current_pos[0] - 1, current_pos[1]
    else:
        return current_pos

def find_number_position(matrix, number):
    rows = len(matrix)
    cols = len(matrix[0])

    for i in range(rows):
        for j in range(cols):
            if matrix[i][j] == number:
                return i, j

    return None

def get_movement(current_pos, target_pos):
    movements = []
    if current_pos[0] < target_pos[0]:
        movements.extend(['d'] * (target_pos[0] - current_pos[0]))
    elif current_pos[0] > target_pos[0]:
        movements.extend(['u'] * (current_pos[0] - target_pos[0]))

    if current_pos[1] < target_pos[1]:
        movements.extend(['r'] * (target_pos[1] - current_pos[1]))
    elif current_pos[1] > target_pos[1]:
        movements.extend(['l'] * (current_pos[1] - target_pos[1]))

    return movements

def main():
    keyboard_matrix = [['1', '2', '3'],
                       ['4', '5', '6'],
                       ['7', '8', '9'],
                       ['del', '0', 'return']]
    current_position = (0, 0)  # Initial position

    print_keyboard_matrix(keyboard_matrix, current_position)

    while True:
        target_number = input("Enter target number: ")

        if target_number == 'exit':
            break

        target_position = find_number_position(keyboard_matrix, target_number)

        if target_position is None:
            print("Target number not found. Please try again.")
            continue

        movements = get_movement(current_position, target_position)

        for movement in movements:
            if movement == 'r':
                current_position = move_right(current_position, len(keyboard_matrix[0]))
            elif movement == 'l':
                current_position = move_left(current_position)
            elif movement == 'd':
                current_position = move_down(current_position, len(keyboard_matrix))
            elif movement == 'u':
                current_position = move_up(current_position)

            print_keyboard_matrix(keyboard_matrix, current_position)

if __name__ == "__main__":
    main()
