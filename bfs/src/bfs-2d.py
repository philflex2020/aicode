from collections import deque

def read_matrix_from_file(filename):
    with open(filename, 'r') as file:
        matrix = [list(map(lambda x: x.lower() == 'true', line.strip().split())) for line in file.readlines()]
    return matrix

def get_coordinates(prompt_message):
    while True:
        try:
            coords = input(prompt_message).split(',')
            if len(coords) != 2:
                raise ValueError("You must enter exactly two coordinates separated by a comma.")
            x, y = int(coords[0]), int(coords[1])
            return x, y
        except ValueError:
            print("Invalid input. Please enter coordinates in the format 'x,y' where x and y are integers.")

def bfs(matrix, start, end):
    directions = [(0,1), (1,0), (0,-1), (-1,0)]
    visited = [[False for _ in range(len(matrix[0]))] for _ in range(len(matrix))]
    queue = deque([start])

    while queue:
        x, y = queue.popleft()
        if (x, y) == end:
            return True
        for dx, dy in directions:
            new_x, new_y = x + dx, y + dy
            if 0 <= new_x < len(matrix) and 0 <= new_y < len(matrix[0]) and not visited[new_x][new_y] and matrix[new_x][new_y]:
                visited[new_x][new_y] = True
                queue.append((new_x, new_y))

    return False

matrix_file = "natrix.txt"
matrix = read_matrix_from_file(matrix_file)

for row in matrix:
    print(row)

start_x, start_y = get_coordinates("Enter start coordinates (x,y): ")
end_x, end_y = get_coordinates("Enter end coordinates (x,y): ")

if bfs(matrix, (start_x, start_y), (end_x, end_y)):
    print(f"A path exists between start point: ({start_x},{start_y}) and end point: ({end_x},{end_y}).")
else:
    print(f"No path exists between start point: ({start_x},{start_y}) and end point: ({end_x},{end_y}).")
