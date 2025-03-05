from flask import Flask, render_template

app = Flask(__name__)

# Sample JSON data
data = [
    {
        "People": [
            {"name": "Alice", "age": 25, "city": "New York"},
            {"name": "Bob", "age": 30, "city": "San Francisco"},
            {"name": "Jimmy", "age": 45, "city": "London"},
            {"name": "Charlie", "age": 35, "city": "Los Angeles"},
        ]
    },
    {
        "Projects": [
            {"name": "Project1", "budget": 100000, "duration": "24 months"},
            {"name": "Project2", "budget": 150000, "duration": "36 months"},
            {"name": "Project3", "budget": 200000, "duration": "12 months"},
        ]
    },
    {
        "Locations": [
            {
                "country": "USA",
                "town": "Apex",
                "state": "North Carolina",
                "date": "10_july_2023",
            },
            {
                "country": "USA",
                "town": "Raleigh",
                "state": "North Carolina",
                "date": "14_aug_2024",
            },
            {
                "country": "UK",
                "town": "Croydon",
                "state": "Surrey",
                "date": "24_sept_1918",
            },
        ]
    },
]


@app.route("/")
def index():
    return render_template("index.html", data=data)


if __name__ == "__main__":
    app.run(debug=True)