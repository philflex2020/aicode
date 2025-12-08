# # Create and activate a virtual environment (example for Windows, adjust for your OS)
# python -m venv venv
# .\venv\Scripts\activate

# # Install the required packages
# pip install fastapi uvicorn jinja2
# uvicorn main:app --reload
#uvicorn main:app --host 0.0.0.0 --port 8501 --reload
import sqlite3
from fastapi import FastAPI, Request, Form, HTTPException
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
import datetime

app = FastAPI()

app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")
DATABASE_URL = "tasks.db"

# Define the initial tasks data (matches updated schema)
INITIAL_TASKS_DATA = [
    ("Item 1", "Created shared memory app, simplified task development and object footprint.", "Completed", "2025-11-01", 8.0, 8.0, 0),
    ("Item 2", "Completed data_path websocket integration for sbmu and rbms.", "Completed", "2025-11-05", 16.0, 15.5, 0),
    ("Item 3", "Wrapped the system with nginx proxies to add encryption to the data path.", "Completed", "2025-11-10", 4.0, 4.0, 0),
    ("Item 4", "Extended metrics to monitor different data and provide tool to configure / setup system.", "In Progress", "2025-11-15", 24.0, 0.0, 0),
]

def get_db_connection():
    conn = sqlite3.connect(DATABASE_URL)
    conn.row_factory = sqlite3.Row
    return conn

def initialize_database():
    conn = get_db_connection()
    cursor = conn.cursor()
    
    # 1. Create table with initial schema
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            work_item TEXT NOT NULL,
            description TEXT NOT NULL,
            status TEXT NOT NULL
        )
    ''')
    conn.commit()

    # 2. Use ALTER TABLE to add new columns if they don't exist (SQLite handles this gracefully)
    try:
        cursor.execute("ALTER TABLE tasks ADD COLUMN task_date TEXT")
        cursor.execute("ALTER TABLE tasks ADD COLUMN est_hours REAL")
        cursor.execute("ALTER TABLE tasks ADD COLUMN actual_hours REAL")
        cursor.execute("ALTER TABLE tasks ADD COLUMN archived INTEGER DEFAULT 0")
        conn.commit()
    except sqlite3.OperationalError:
        # Columns already exist, which is fine
        pass

    # 3. Check if table is empty of non-archived tasks and populate
    cursor.execute("SELECT COUNT(*) FROM tasks WHERE archived = 0")
    count = cursor.fetchone()[0]

    if count == 0:
        cursor.executemany(
            "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours, archived) VALUES (?, ?, ?, ?, ?, ?, ?)",
            INITIAL_TASKS_DATA
        )
        conn.commit()
        print("Database initialized with sample tasks.")
    else:
        print("Database already contains data; skipping initialization.")

    conn.close()

initialize_database()

@app.get("/", response_class=HTMLResponse)
async def read_tasks(request: Request):
    conn = get_db_connection()
    cursor = conn.cursor()
    # Select only non-archived tasks
    cursor.execute("SELECT * FROM tasks WHERE archived = 0 ORDER BY task_date DESC, id DESC")
    tasks = cursor.fetchall()
    
    completed_count = sum(1 for task in tasks if task['status'] == "Completed")
    total_count = len(tasks)
    progress_percentage = (completed_count / total_count) * 100 if total_count > 0 else 0
    report_date = datetime.date.today().strftime("%B %d, %Y")

    conn.close()

    return templates.TemplateResponse(
        "tasks.html",
        {
            "request": request,
            "tasks": tasks,
            "report_date": report_date,
            "progress_percentage": int(progress_percentage),
            "completed_count": completed_count,
            "total_count": total_count,
            "today_date": datetime.date.today().isoformat() # Used for default date in new task form
        }
    )

@app.post("/add_task", response_class=RedirectResponse)
async def add_task(
    work_item: str = Form(...), 
    description: str = Form(...),
    task_date: str = Form(...),
    est_hours: float = Form(0.0)
):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours, archived) VALUES (?, ?, ?, ?, ?, ?, ?)",
        (work_item, description, "In Progress", task_date, est_hours, 0.0, 0)
    )
    conn.commit()
    conn.close()
    return RedirectResponse(url="/", status_code=303)

@app.post("/update_task/{task_id}", response_class=RedirectResponse)
async def update_task(
    task_id: int, 
    status: str = Form(...),
    actual_hours: float = Form(...)
):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute(
        "UPDATE tasks SET status = ?, actual_hours = ? WHERE id = ?",
        (status, actual_hours, task_id)
    )
    conn.commit()
    conn.close()

    if cursor.rowcount == 0:
        raise HTTPException(status_code=404, detail="Task not found")

    return RedirectResponse(url="/", status_code=303)

@app.post("/archive_task/{task_id}", response_class=RedirectResponse)
async def archive_task(task_id: int):
    conn = get_db_connection()
    cursor = conn.cursor()
    # Set archived status to 1 (True)
    cursor.execute(
        "UPDATE tasks SET archived = 1 WHERE id = ?",
        (task_id,)
    )
    conn.commit()
    conn.close()
    return RedirectResponse(url="/", status_code=303)

@app.post("/delete_task/{task_id}", response_class=RedirectResponse)
async def delete_task(task_id: int):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute(
        "DELETE FROM tasks WHERE id = ?",
        (task_id,)
    )
    conn.commit()
    conn.close()
    return RedirectResponse(url="/", status_code=303)
