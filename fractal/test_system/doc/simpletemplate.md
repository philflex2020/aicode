Here's a simple, serverless HTML template that automatically refreshes every 10 seconds and displays data from a JSON object in a table format. The example uses a hardcoded JSON array to populate the table, which you can replace with dynamic data loaded from another source if necessary.

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="10"> <!-- Refresh the page every 10 seconds -->
    <title>Data Table Display</title>
    <style>
        table, th, td {
            border: 1px solid black;
            border-collapse: collapse;
            padding: 8px;
            text-align: left;
        }
    </style>
</head>
<body>
    <h1>Data Table</h1>
    <table id="dataTable">
        <thead>
            <tr>
                <!-- Header will be populated dynamically -->
            </tr>
        </thead>
        <tbody>
            <!-- Data will be populated dynamically -->
        </tbody>
    </table>

    <script>
        // Example JSON data
        var jsonData = [
            {"name": "Alice", "age": 25, "city": "New York"},
            {"name": "Bob", "age": 30, "city": "San Francisco"},
            {"name": "Charlie", "age": 35, "city": "Los Angeles"}
        ];

        function loadTableData(items) {
            const table = document.getElementById('dataTable');
            const tableHead = table.querySelector('thead tr');
            const tableBody = table.querySelector('tbody');

            // Clear existing table data
            tableHead.innerHTML = '';
            tableBody.innerHTML = '';

            // Populate header
            if (items.length > 0) {
                Object.keys(items[0]).forEach(key => {
                    let headerCell = document.createElement('th');
                    headerCell.textContent = key.charAt(0).toUpperCase() + key.slice(1); // Capitalize headers
                    tableHead.appendChild(headerCell);
                });
            }

            // Populate rows
            items.forEach(item => {
                let row = document.createElement('tr');
                Object.values(item).forEach(text => {
                    let cell = document.createElement('td');
                    cell.textContent = text;
                    row.appendChild(cell);
                });
                tableBody.appendChild(row);
            });
        }

        // Initial load of data
        loadTableData(jsonData);

        // Optionally, if you want to fetch data periodically from a server or file:
        // setInterval(function() {
        //     fetch('path_to_your_json_data.json')
        //         .then(response => response.json())
        //         .then(data => loadTableData(data));
        // }, 10000); // refresh data every 10 seconds
    </script>
</body>
</html>
```

### Key Features:

1. **Automatic Refresh:** The webpage automatically refreshes every 10 seconds, ensuring that the data displayed is up-to-date.
2. **Dynamic Table Creation:** The table headers and rows are dynamically created based on the JSON object structure, making it generic and capable of displaying any array of objects.
3. **Basic Styling:** The table is styled with minimal CSS to improve readability and appearance.
4. **Optional Data Fetching:** While this example uses static JSON data, you can uncomment the `setInterval` function and use `fetch` to load data from a server or local file periodically.

You can modify this template to fetch data dynamically if you have a specific server or file from which you can load JSON data. This setup is ideal for a lightweight, serverless environment where you want to display data that changes regularly without needing a backend server to serve the HTML page.