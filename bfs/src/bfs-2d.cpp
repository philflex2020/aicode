#include <iostream>
#include <vector>
#include <queue>
#include <utility>

using namespace std;

// Directions: Up, Down, Left, Right
const int dx[4] = {-1, 1, 0, 0};
const int dy[4] = {0, 0, -1, 1};

bool isValid(int x, int y, int rows, int cols, const vector<vector<bool>>& matrix, vector<vector<bool>>& visited) {
    return (x >= 0 && x < rows && y >= 0 && y < cols && matrix[x][y] && !visited[x][y]);
}

vector<pair<int, int>> bfs(const vector<vector<bool>>& matrix, pair<int, int> start, pair<int, int> dest) {
    int rows = matrix.size();
    int cols = matrix[0].size();
    vector<vector<bool>> visited(rows, vector<bool>(cols, false));
    vector<vector<pair<int, int>>> parent(rows, vector<pair<int, int>>(cols, {-1, -1}));

    queue<pair<int, int>> q;
    q.push(start);
    visited[start.first][start.second] = true;

    while (!q.empty()) {
        pair<int, int> curr = q.front();
        q.pop();

        if (curr == dest) {
            vector<pair<int, int>> path;
            for (; curr != make_pair(-1, -1); curr = parent[curr.first][curr.second]) {
                path.push_back(curr);
            }
            reverse(path.begin(), path.end());
            return path;
        }

        for (int i = 0; i < 4; ++i) {
            int newX = curr.first + dx[i];
            int newY = curr.second + dy[i];
            if (isValid(newX, newY, rows, cols, matrix, visited)) {
                visited[newX][newY] = true;
                parent[newX][newY] = curr;
                q.push({newX, newY});
            }
        }
    }

    return {}; // Return empty path if no path found
}

int main() {
    int rows, cols;
    cout << "Enter number of rows and columns: ";
    cin >> rows >> cols;

    vector<vector<bool>> matrix(rows, vector<bool>(cols));
    cout << "Enter the matrix (1 for True, 0 for False):\n";
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            int val;
            cin >> val;
            matrix[i][j] = val == 1;
        }
    }

    pair<int, int> start, dest;
    cout << "Enter starting coordinates (row and column): ";
    cin >> start.first >> start.second;

    cout << "Enter destination coordinates (row and column): ";
    cin >> dest.first >> dest.second;

    vector<pair<int, int>> path = bfs(matrix, start, dest);
    if (path.empty()) {
        cout << "No path found!\n";
    } else {
        cout << "Shortest path:\n";
        for (auto& p : path) {
            cout << "(" << p.first << ", " << p.second << ")\n";
        }
    }

    return 0;
}
