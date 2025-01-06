Below is a C++ program to create a 2D map using 3 scans taken at 3 different locations. The scans will be merged into a global coordinate system to form a map of objects in 2D space.

### Implementation

```cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

struct Point {
    float x, y; // Cartesian coordinates
};

struct RobotPose {
    float x, y;      // Robot's position in global coordinates
    float theta;     // Robot's orientation in degrees
};

struct RangeData {
    int angle;      // Angle in degrees relative to the robot
    float distance; // Distance measurement
};

// Convert polar coordinates to Cartesian coordinates
Point polar_to_cartesian(float distance, float angle_deg, float robot_x, float robot_y, float robot_theta) {
    float angle_rad = (angle_deg + robot_theta) * M_PI / 180.0;
    float x = robot_x + distance * cos(angle_rad);
    float y = robot_y + distance * sin(angle_rad);
    return {x, y};
}

// Perform a scan (simulated for this example)
std::vector<RangeData> perform_scan(int step_angle) {
    std::vector<RangeData> scan_data;
    for (int angle = 0; angle < 360; angle += step_angle) {
        // Simulate distance with some random noise (replace with actual sensor data)
        float distance = 100.0 + static_cast<float>(rand() % 20 - 10); // 100 +/- 10
        scan_data.push_back({angle, distance});
    }
    return scan_data;
}

// Process multiple scans to generate a 2D map
std::vector<Point> generate_2d_map(const std::vector<RobotPose>& robot_poses, int step_angle) {
    std::vector<Point> map_points;

    for (const auto& pose : robot_poses) {
        // Perform a scan at this location
        auto scan_data = perform_scan(step_angle);

        // Transform each point to global coordinates
        for (const auto& data : scan_data) {
            if (data.distance > 0) { // Ignore invalid distances
                Point global_point = polar_to_cartesian(data.distance, data.angle, pose.x, pose.y, pose.theta);
                map_points.push_back(global_point);
            }
        }
    }

    return map_points;
}

// Print the map points
void print_map(const std::vector<Point>& map_points) {
    std::cout << "2D Map Points (x, y):" << std::endl;
    for (const auto& point : map_points) {
        std::cout << "(" << point.x << ", " << point.y << ")" << std::endl;
    }
}

int main() {
    // Define 3 robot poses (locations)
    std::vector<RobotPose> robot_poses = {
        {0.0, 0.0, 0.0},  // First scan at origin, facing north
        {200.0, 0.0, 90.0}, // Second scan 200 units east, facing east
        {200.0, 200.0, 180.0} // Third scan 200 units north of the second location, facing south
    };

    int step_angle = 10; // Angular resolution of the scan

    // Generate the 2D map
    auto map_points = generate_2d_map(robot_poses, step_angle);

    // Print the map points
    print_map(map_points);

    return 0;
}
```

---

### Explanation

1. **`polar_to_cartesian`**:
   - Converts a polar coordinate (angle and distance) relative to the robot into a global Cartesian coordinate.
   - Accounts for the robot's position (`robot_x`, `robot_y`) and orientation (`robot_theta`).

2. **Simulated Scans (`perform_scan`)**:
   - Simulates range data for the full 360° field of view, with measurements taken at `step_angle` intervals.

3. **`generate_2d_map`**:
   - Processes scans from each robot position.
   - Transforms all measured points into global coordinates and adds them to a shared map.

4. **Print the Map**:
   - Outputs the computed 2D map as a list of `(x, y)` coordinates.

---

### Example Output

For simulated scans at the three locations:

```
2D Map Points (x, y):
(100.0, 0.0)
(99.2, 17.4)
(96.2, 34.2)
...
(300.0, 200.0)
(283.6, 200.0)
...
```

---

### Notes

1. **Enhancements**:
   - Replace `perform_scan` with actual ultrasonic sensor readings.
   - Add filtering to reduce noise and improve map accuracy.

2. **Visualization**:
   - Use a library like OpenCV (C++) or matplotlib (Python) to visualize the 2D map.

3. **Localization**:
   - If the robot’s positions are estimated (e.g., via odometry), compensate for localization errors to maintain map accuracy.

This code provides a foundational framework to integrate sensor data and create a 2D map of the environment. Let me know if you’d like help extending this further!