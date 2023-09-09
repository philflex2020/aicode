#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

vector<vector<bool>> read_matrix_from_file(const string& filename) {
    ifstream file(filename);
    string line;
    vector<vector<bool>> matrix;
    
    while (getline(file, line)) {
        vector<bool> row;
        string word;
        for (int i = 0; i < line.size(); i++) {
            if (line[i] == ' ') {
                row.push_back(word == "true");
                word = "";
            } else {
                word += line[i];
            }
        }
        row.push_back(word == "true");  // Add the last word in the line
        matrix.push_back(row);
    }
    return matrix;
}

pair<int, int> get_coordinates(const string& prompt_message) {
    int x, y;
    cout << prompt_message;
    cin >> x >> y;
    return {x, y};
}

void dfs(const vector<vector<bool>>& matrix, int x, int y, const pair<int, int>& end, vector<pair<int, int>>& path, vector<vector<pair<int, int>>>& all_paths) {
    if (x < 0 || x >= matrix.size() || y < 0 || y >= matrix[0].size() || !matrix[x][y])
        return;

    path.push_back({x, y});

    if (x == end.first && y == end.second) {
        all_paths.push_back(path);
    } else {
        vector<vector<bool>> matrix_copy = matrix;
        matrix_copy[x][y] = false;
        dfs(matrix_copy, x+1, y, end, path, all_paths);
        dfs(matrix_copy, x-1, y, end, path, all_paths);
        dfs(matrix_copy, x, y+1, end, path, all_paths);
        dfs(matrix_copy, x, y-1, end, path, all_paths);
    }
    
    path.pop_back();
}

int main() {
    string matrix_file = "matrix.txt";
    vector<vector<bool>> matrix = read_matrix_from_file(matrix_file);

    for (const auto& row : matrix) {
        for (bool val : row)
            cout << (val ? "true " : "false ");
        cout << endl;
    }

    pair<int, int> start = get_coordinates("Enter start coordinates (x y): ");
    pair<int, int> end = get_coordinates("Enter end coordinates (x y): ");

    vector<pair<int, int>> current_path;
    vector<vector<pair<int, int>>> all_paths;
    
    dfs(matrix, start.first, start.second, end, current_path, all_paths);

    if (all_paths.empty()) {
        cout << "No path exists between the given points." << endl;
    } else {
        auto shortest_path = min_element(all_paths.begin(), all_paths.end(), [](const auto& a, const auto& b) {
            return a.size() < b.size();
        });

        cout << "Shortest path between the points is:" << endl;
        for (const auto& coord : *shortest_path) {
            cout << "(" << coord.first << "," << coord.second << ") -> ";
        }
        cout << "end" << endl;
    }

    return 0;
}
