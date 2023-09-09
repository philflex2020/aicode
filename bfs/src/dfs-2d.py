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

def dfs(matrix, start, end, path=[]):
    if start == end:
        return [path + [end]]

    x, y = start
    if x < 0 or x >= len(matrix) or y < 0 or y >= len(matrix[0]) or not matrix[x][y]:
        return []

    matrix[x][y] = False  # Mark as visited
    paths = []
    for dx, dy in [(0,1), (1,0), (0,-1), (-1,0)]:
        new_x, new_y = x + dx, y + dy
        paths += dfs(matrix, (new_x, new_y), end, path + [start])
    matrix[x][y] = True  # Mark as unvisited for other paths

    return paths

matrix_file = "matrix.txt"
matrix = read_matrix_from_file(matrix_file)

for row in matrix:
    print(row)

start_x, start_y = get_coordinates("Enter start coordinates (x,y): ")
end_x, end_y = get_coordinates("Enter end coordinates (x,y): ")

paths = dfs(matrix, (start_x, start_y), (end_x, end_y))
if not paths:
    print(f"No path exists between start point: ({start_x},{start_y}) and end point: ({end_x},{end_y}).")
else:
    print(paths)
    shortest_path = min(paths, key=len)
    print(f"Shortest path between start point: ({start_x},{start_y}) and end point: ({end_x},{end_y}) is:")
    print(shortest_path)
