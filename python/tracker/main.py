# # Create and activate a virtual environment (example for Windows, adjust for your OS)
# python -m venv venv
# .\venv\Scripts\activate

# # Install the required packages
# pip install fastapi uvicorn jinja2
# uvicorn main:app --reload
#uvicorn main:app --host 0.0.0.0 --port 8501 --reload
import sqlite3
from fastapi import FastAPI, Request, Form, HTTPException, Query
from fastapi.responses import HTMLResponse, RedirectResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
import datetime
import os

app = FastAPI()
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

# Define database file names
DB_ACTIVE = "active.db"
DB_ARCHIVE = "archive.db"

# Initial data
INITIAL_TASKS_DATA = [
    ("Item 1", "Created shared memory app, simplified task development and object footprint.", "Completed", "2025-11-01", 8.0, 8.0),
    ("Item 2", "Completed data_path websocket integration for sbmu and rbms.", "Completed", "2025-11-05", 16.0, 15.5),
    ("Item 3", "Wrapped the system with nginx proxies to add encryption to the data path.", "Completed", "2025-11-10", 4.0, 4.0),
    ("Item 4", "Extended metrics to monitor different data and provide tool to configure / setup system.", "In Progress", "2025-11-15", 24.0, 0.0),
]

def get_db_connection(db_path: str):
    """Establish a database connection and enable row_factory for name access."""
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn

def create_table(conn):
    """Creates the tasks table in a given connection."""
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            work_item TEXT NOT NULL,
            description TEXT NOT NULL,
            status TEXT NOT NULL,
            task_date TEXT,
            est_hours REAL,
            actual_hours REAL
        )
    ''')
    conn.commit()

def initialize_database():
    """Create databases and populate active.db if empty."""
    # Ensure both DB files exist and have the table structure
    for db_file in [DB_ACTIVE, DB_ARCHIVE]:
        conn = get_db_connection(db_file)
        create_table(conn)
        conn.close()

    # Check if active DB is empty and populate
    conn = get_db_connection(DB_ACTIVE)
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM tasks")
    count = cursor.fetchone()[0]

    if count == 0:
        cursor.executemany(
            "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours) VALUES (?, ?, ?, ?, ?, ?)",
            INITIAL_TASKS_DATA
        )
        conn.commit()
        print("Active database initialized with sample tasks.")
    conn.close()

initialize_database()

@app.get("/", response_class=HTMLResponse)
async def read_tasks(request: Request, view_db: str = Query(DB_ACTIVE)):
    """Render the tasks page, defaulting to active tasks."""
    
    # Ensure the requested DB is valid
    if view_db not in [DB_ACTIVE, DB_ARCHIVE]:
        raise HTTPException(status_code=400, detail="Invalid database specified.")

    conn = get_db_connection(view_db)
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM tasks ORDER BY task_date DESC, id DESC")
    tasks = cursor.fetchall()
    conn.close()
    
    # Calculate stats
    completed_count = sum(1 for task in tasks if task['status'] == "Completed")
    total_count = len(tasks)
    progress_percentage = (completed_count / total_count) * 100 if total_count > 0 else 0
    report_date = datetime.date.today().strftime("%B %d, %Y")

    return templates.TemplateResponse(
        "tasks.html",
        {
            "request": request,
            "tasks": tasks,
            "report_date": report_date,
            "progress_percentage": int(progress_percentage),
            "completed_count": completed_count,
            "total_count": total_count,
            "today_date": datetime.date.today().isoformat(),
            "current_db": view_db # Pass the current DB name to the template
        }
    )

# Endpoint to fetch a single task's data for the modal (using GET for simplicity/API use)
@app.get("/task_details/{task_id}", response_class=JSONResponse)
async def get_task_details(task_id: int, view_db: str = Query(DB_ACTIVE)):
    if view_db not in [DB_ACTIVE, DB_ARCHIVE]:
        raise HTTPException(status_code=400, detail="Invalid database specified.")
        
    conn = get_db_connection(view_db)
    cursor = conn.cursor()
    # Need to specify table name in query because conn.row_factory makes row[0] ambiguous if we return full row
    cursor.execute("SELECT * FROM tasks WHERE id = ?", (task_id,))
    task = cursor.fetchone()
    conn.close()
    
    if task is None:
        raise HTTPException(status_code=404, detail="Task not found")
    return dict(task)


@app.post("/add_task", response_class=RedirectResponse)
async def add_task(
    work_item: str = Form(...), 
    description: str = Form(...),
    task_date: str = Form(...),
    est_hours: float = Form(0.0)
):
    """Handle task submission (always adds to active.db)."""
    conn = get_db_connection(DB_ACTIVE)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours) VALUES (?, ?, ?, ?, ?, ?)",
        (work_item, description, "In Progress", task_date, est_hours, 0.0)
    )
    conn.commit()
    conn.close()
    return RedirectResponse(url=f"/?view_db={DB_ACTIVE}", status_code=303)

@app.post("/update_task/{task_id}", response_class=RedirectResponse)
async def update_task(
    task_id: int, 
    work_item: str = Form(...),
    description: str = Form(...),
    status: str = Form(...),
    actual_hours: float = Form(...),
    current_db: str = Form(DB_ACTIVE)
):
    """Update all fields of a specific task via the modal form."""
    if current_db not in [DB_ACTIVE, DB_ARCHIVE]:
        raise HTTPException(status_code=400, detail="Invalid database specified.")
        
    conn = get_db_connection(current_db)
    cursor = conn.cursor()
    cursor.execute(
        "UPDATE tasks SET work_item = ?, description = ?, status = ?, actual_hours = ? WHERE id = ?",
        (work_item, description, status, actual_hours, task_id)
    )
    conn.commit()
    conn.close()

    if cursor.rowcount == 0:
        raise HTTPException(status_code=404, detail="Task not found")

    return RedirectResponse(url=f"/?view_db={current_db}", status_code=303)

@app.post("/move_task/{task_id}", response_class=RedirectResponse)
async def move_task(task_id: int, source_db: str = Form(...)):
    """Move a task from source_db to the other database (archive <-> active)."""
    if source_db not in [DB_ACTIVE, DB_ARCHIVE]:
        raise HTTPException(status_code=400, detail="Invalid source database specified.")

    target_db = DB_ARCHIVE if source_db == DB_ACTIVE else DB_ACTIVE

    conn_source = get_db_connection(source_db)
    cursor_source = conn_source.cursor()
    cursor_source.execute("SELECT * FROM tasks WHERE id = ?", (task_id,))
    task = cursor_source.fetchone()

    if not task:
        conn_source.close()
        raise HTTPException(status_code=404, detail="Task not found in source database.")
    
    # Insert into target DB
    conn_target = get_db_connection(target_db)
    cursor_target = conn_target.cursor()
    cursor_target.execute(
        "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours) VALUES (?, ?, ?, ?, ?, ?)",
        (task['work_item'], task['description'], task['status'], task['task_date'], task['est_hours'], task['actual_hours'])
    )
    conn_target.commit()
    conn_target.close()

    # Delete from source DB
    cursor_source.execute("DELETE FROM tasks WHERE id = ?", (task_id,))
    conn_source.commit()
    conn_source.close()

    # Redirect back to the source DB view
    return RedirectResponse(url=f"/?view_db={source_db}", status_code=303)


@app.post("/delete_task/{task_id}", response_class=RedirectResponse)
async def delete_task(task_id: int, current_db: str = Form(DB_ACTIVE)):
    """Permanently delete a task from the database."""
    if current_db not in [DB_ACTIVE, DB_ARCHIVE]:
        raise HTTPException(status_code=400, detail="Invalid database specified.")
        
    conn = get_db_connection(current_db)
    cursor = conn.cursor()
    cursor.execute("DELETE FROM tasks WHERE id = ?", (task_id,))
    conn.commit()
    conn.close()
    
    if cursor.rowcount == 0:
        raise HTTPException(status_code=404, detail="Task not found")
        
    return RedirectResponse(url=f"/?view_db={current_db}", status_code=303)


@app.post("/hard_reset", response_class=RedirectResponse)
async def hard_reset():
    """Deletes both database files and re-initializes them."""
    if os.path.exists(DB_ACTIVE):
        os.remove(DB_ACTIVE)
    if os.path.exists(DB_ARCHIVE):
        os.remove(DB_ARCHIVE)
    
    initialize_database()
    return RedirectResponse(url=f"/?view_db={DB_ACTIVE}", status_code=303)


# import sqlite3
# from fastapi import FastAPI, Request, Form, HTTPException
# from fastapi.responses import HTMLResponse, RedirectResponse
# from fastapi.staticfiles import StaticFiles
# from fastapi.templating import Jinja2Templates
# import datetime

# app = FastAPI()

# app.mount("/static", StaticFiles(directory="static"), name="static")
# templates = Jinja2Templates(directory="templates")
# DATABASE_URL = "tasks.db"

# # Define the initial tasks data (matches updated schema)
# # Note the '1' at the end for the 'active' status
# INITIAL_TASKS_DATA = [
#     ("Item 1", "Created shared memory app, simplified task development and object footprint.", "Completed", "2025-11-01", 8.0, 8.0, 1),
#     ("Item 2", "Completed data_path websocket integration for sbmu and rbms.", "Completed", "2025-11-05", 16.0, 15.5, 1),
#     ("Item 3", "Wrapped the system with nginx proxies to add encryption to the data path.", "Completed", "2025-11-10", 4.0, 4.0, 1),
#     ("Item 4", "Extended metrics to monitor different data and provide tool to configure / setup system.", "In Progress", "2025-11-15", 24.0, 0.0, 1),
# ]

# def get_db_connection():
#     conn = sqlite3.connect(DATABASE_URL)
#     conn.row_factory = sqlite3.Row
#     return conn

# def initialize_database():
#     conn = get_db_connection()
#     cursor = conn.cursor()
    
#     # Create table with initial schema
#     cursor.execute('''
#         CREATE TABLE IF NOT EXISTS tasks (
#             id INTEGER PRIMARY KEY AUTOINCREMENT,
#             work_item TEXT NOT NULL,
#             description TEXT NOT NULL,
#             status TEXT NOT NULL,
#             task_date TEXT,
#             est_hours REAL,
#             actual_hours REAL,
#             active INTEGER DEFAULT 1
#         )
#     ''')
#     conn.commit()

#     # Check if table is empty of active tasks and populate
#     cursor.execute("SELECT COUNT(*) FROM tasks WHERE active = 1")
#     count = cursor.fetchone()[0]

#     if count == 0:
#         cursor.executemany(
#             "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours, active) VALUES (?, ?, ?, ?, ?, ?, ?)",
#             INITIAL_TASKS_DATA
#         )
#         conn.commit()
#         print("Database initialized with sample tasks.")
    
#     conn.close()

# initialize_database()

# @app.get("/", response_class=HTMLResponse)
# async def read_tasks(request: Request):
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     # Select only active tasks (active = 1), ordered by date
#     cursor.execute("SELECT * FROM tasks WHERE active = 1 ORDER BY task_date DESC, id DESC")
#     tasks = cursor.fetchall()
    
#     completed_count = sum(1 for task in tasks if task['status'] == "Completed")
#     total_count = len(tasks)
#     progress_percentage = (completed_count / total_count) * 100 if total_count > 0 else 0
#     report_date = datetime.date.today().strftime("%B %d, %Y")

#     conn.close()

#     return templates.TemplateResponse(
#         "tasks.html",
#         {
#             "request": request,
#             "tasks": tasks,
#             "report_date": report_date,
#             "progress_percentage": int(progress_percentage),
#             "completed_count": completed_count,
#             "total_count": total_count,
#             "today_date": datetime.date.today().isoformat()
#         }
#     )

# # Endpoint to fetch a single task's data for the modal (using GET for simplicity/API use)
# @app.get("/task_details/{task_id}")
# async def get_task_details(task_id: int):
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute("SELECT * FROM tasks WHERE id = ? AND active = 1", (task_id,))
#     task = cursor.fetchone()
#     conn.close()
#     if task is None:
#         raise HTTPException(status_code=404, detail="Task not found or inactive")
#     # Return as dictionary for JSON response
#     return dict(task)

# @app.post("/add_task", response_class=RedirectResponse)
# async def add_task(
#     work_item: str = Form(...), 
#     description: str = Form(...),
#     task_date: str = Form(...),
#     est_hours: float = Form(0.0)
# ):
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute(
#         "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours, active) VALUES (?, ?, ?, ?, ?, ?, ?)",
#         (work_item, description, "In Progress", task_date, est_hours, 0.0, 1) # Set active to 1
#     )
#     conn.commit()
#     conn.close()
#     return RedirectResponse(url="/", status_code=303)

# @app.post("/update_task/{task_id}", response_class=RedirectResponse)
# async def update_task(
#     task_id: int, 
#     work_item: str = Form(...),
#     description: str = Form(...),
#     status: str = Form(...),
#     actual_hours: float = Form(...)
# ):
#     """Update all fields of a specific task via the modal form."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute(
#         "UPDATE tasks SET work_item = ?, description = ?, status = ?, actual_hours = ? WHERE id = ? AND active = 1",
#         (work_item, description, status, actual_hours, task_id)
#     )
#     conn.commit()
#     conn.close()

#     if cursor.rowcount == 0:
#         raise HTTPException(status_code=404, detail="Task not found or inactive")

#     return RedirectResponse(url="/", status_code=303)

# @app.post("/set_task_active_status/{task_id}", response_class=RedirectResponse)
# async def set_task_active_status(task_id: int, active_status: int = Form(...)):
#     """Sets a task as active (1) or inactive (0) / archived."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute(
#         "UPDATE tasks SET active = ? WHERE id = ?",
#         (active_status, task_id,)
#     )
#     conn.commit()
#     conn.close()
    
#     if cursor.rowcount == 0:
#         raise HTTPException(status_code=404, detail="Task not found")

#     return RedirectResponse(url="/", status_code=303)

# @app.post("/delete_task/{task_id}", response_class=RedirectResponse)
# async def delete_task(task_id: int):
#     """Permanently delete a task from the database."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute("DELETE FROM tasks WHERE id = ?", (task_id,))
#     conn.commit()
#     conn.close()
    
#     if cursor.rowcount == 0:
#         raise HTTPException(status_code=404, detail="Task not found")
        
#     return RedirectResponse(url="/", status_code=303)

####################################################

# import sqlite3
# from fastapi import FastAPI, Request, Form, HTTPException
# from fastapi.responses import HTMLResponse, RedirectResponse
# from fastapi.staticfiles import StaticFiles
# from fastapi.templating import Jinja2Templates
# import datetime

# app = FastAPI()

# app.mount("/static", StaticFiles(directory="static"), name="static")
# templates = Jinja2Templates(directory="templates")
# DATABASE_URL = "tasks.db"

# # Define the initial tasks data (matches updated schema)
# INITIAL_TASKS_DATA = [
#     ("Item 1", "Created shared memory app, simplified task development and object footprint.", "Completed", "2025-11-01", 8.0, 8.0, 0),
#     ("Item 2", "Completed data_path websocket integration for sbmu and rbms.", "Completed", "2025-11-05", 16.0, 15.5, 0),
#     ("Item 3", "Wrapped the system with nginx proxies to add encryption to the data path.", "Completed", "2025-11-10", 4.0, 4.0, 0),
#     ("Item 4", "Extended metrics to monitor different data and provide tool to configure / setup system.", "In Progress", "2025-11-15", 24.0, 0.0, 0),
# ]

# def get_db_connection():
#     """Establish a database connection and enable row_factory for name access."""
#     conn = sqlite3.connect(DATABASE_URL)
#     conn.row_factory = sqlite3.Row  
#     return conn

# def initialize_database():
#     """
#     Create the tasks table, add necessary columns, and populate
#     with initial data if the table is empty.
#     """
#     conn = get_db_connection()
#     cursor = conn.cursor()
    
#     # 1. Create table with initial schema
#     cursor.execute('''
#         CREATE TABLE IF NOT EXISTS tasks (
#             id INTEGER PRIMARY KEY AUTOINCREMENT,
#             work_item TEXT NOT NULL,
#             description TEXT NOT NULL,
#             status TEXT NOT NULL
#         )
#     ''')
#     conn.commit()

#     # 2. Use ALTER TABLE to add new columns if they don't exist
#     try:
#         cursor.execute("ALTER TABLE tasks ADD COLUMN task_date TEXT")
#         cursor.execute("ALTER TABLE tasks ADD COLUMN est_hours REAL")
#         cursor.execute("ALTER TABLE tasks ADD COLUMN actual_hours REAL")
#         cursor.execute("ALTER TABLE tasks ADD COLUMN archived INTEGER DEFAULT 0")
#         conn.commit()
#     except sqlite3.OperationalError:
#         # Columns already exist, ignore error
#         pass

#     # 3. Check if table is empty of non-archived tasks and populate
#     cursor.execute("SELECT COUNT(*) FROM tasks WHERE archived = 0")
#     count = cursor.fetchone()[0] # Access the count value

#     if count == 0:
#         cursor.executemany(
#             "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours, archived) VALUES (?, ?, ?, ?, ?, ?, ?)",
#             INITIAL_TASKS_DATA
#         )
#         conn.commit()
#         print("Database initialized with sample tasks.")
#     # If the table isn't empty, but some columns might be null from prior versions, 
#     # we could add an update step here, but for this simple app, we assume initialization covers it.

#     conn.close()

# # Run the initialization function when the application starts up
# initialize_database()

# @app.get("/", response_class=HTMLResponse)
# async def read_tasks(request: Request):
#     """Render the main tasks page with data from the database."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     # Select only non-archived tasks, ordered by date
#     cursor.execute("SELECT * FROM tasks WHERE archived = 0 ORDER BY task_date DESC, id DESC")
#     tasks = cursor.fetchall()
    
#     # Calculate stats
#     completed_count = sum(1 for task in tasks if task['status'] == "Completed")
#     total_count = len(tasks)
#     progress_percentage = (completed_count / total_count) * 100 if total_count > 0 else 0
#     report_date = datetime.date.today().strftime("%B %d, %Y")

#     conn.close()

#     return templates.TemplateResponse(
#         "tasks.html",
#         {
#             "request": request,
#             "tasks": tasks,
#             "report_date": report_date,
#             "progress_percentage": int(progress_percentage),
#             "completed_count": completed_count,
#             "total_count": total_count,
#             "today_date": datetime.date.today().isoformat() # Used for default date in new task form
#         }
#     )

# @app.post("/add_task", response_class=RedirectResponse)
# async def add_task(
#     work_item: str = Form(...), 
#     description: str = Form(...),
#     task_date: str = Form(...),
#     est_hours: float = Form(0.0)
# ):
#     """Handle task submission."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute(
#         "INSERT INTO tasks (work_item, description, status, task_date, est_hours, actual_hours, archived) VALUES (?, ?, ?, ?, ?, ?, ?)",
#         (work_item, description, "In Progress", task_date, est_hours, 0.0, 0)
#     )
#     conn.commit()
#     conn.close()
#     return RedirectResponse(url="/", status_code=303)

# @app.post("/update_task/{task_id}", response_class=RedirectResponse)
# async def update_task(
#     task_id: int, 
#     status: str = Form(...),
#     actual_hours: float = Form(...)
# ):
#     """Update the status and actual hours of a specific task."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute(
#         "UPDATE tasks SET status = ?, actual_hours = ? WHERE id = ?",
#         (status, actual_hours, task_id)
#     )
#     conn.commit()
#     conn.close()

#     if cursor.rowcount == 0:
#         raise HTTPException(status_code=404, detail="Task not found")

#     return RedirectResponse(url="/", status_code=303)

# @app.post("/archive_task/{task_id}", response_class=RedirectResponse)
# async def archive_task(task_id: int):
#     """Mark a task as archived (hides it from the main view)."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     # Set archived status to 1 (True)
#     cursor.execute(
#         "UPDATE tasks SET archived = 1 WHERE id = ?",
#         (task_id,)
#     )
#     conn.commit()
#     conn.close()
    
#     if cursor.rowcount == 0:
#         raise HTTPException(status_code=404, detail="Task not found")

#     return RedirectResponse(url="/", status_code=303)

# @app.post("/delete_task/{task_id}", response_class=RedirectResponse)
# async def delete_task(task_id: int):
#     """Permanently delete a task from the database."""
#     conn = get_db_connection()
#     cursor = conn.cursor()
#     cursor.execute(
#         "DELETE FROM tasks WHERE id = ?",
#         (task_id,)
#     )
#     conn.commit()
#     conn.close()
    
#     if cursor.rowcount == 0:
#         raise HTTPException(status_code=404, detail="Task not found")
        
#     return RedirectResponse(url="/", status_code=303)
