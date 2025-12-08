from fastapi import FastAPI, Request, Form, HTTPException
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel
import datetime
from typing import List

app = FastAPI()

# Mount static files (CSS)
app.mount("/static", StaticFiles(directory="static"), name="static")

# Configure Jinja2 templates
templates = Jinja2Templates(directory="templates")

# In-memory database to store tasks
# In a production app, you would use a real database (e.g., PostgreSQL, SQLite)
class Task(BaseModel):
    id: int
    work_item: str
    description: str
    status: str  # e.g., "Completed", "In Progress"

tasks_db: List[Task] = [
    Task(id=1, work_item="Item 1", description="Created shared memory app, simplified task development and object footprint.", status="Completed"),
    Task(id=2, work_item="Item 2", description="Completed data_path websocket integration for sbmu and rbms.", status="Completed"),
    Task(id=3, work_item="Item 3", description="Wrapped the system with nginx proxies to add encryption to the data path.", status="Completed"),
    Task(id=4, work_item="Item 4", description="Extended metrics to monitor different data and provide tool to configure / setup system.", status="In Progress"),
]

@app.get("/", response_class=HTMLResponse)
async def read_tasks(request: Request):
    """Render the main tasks page with current data."""
    completed_count = sum(1 for task in tasks_db if task.status == "Completed")
    total_count = len(tasks_db)
    progress_percentage = (completed_count / total_count) * 100 if total_count > 0 else 0
    report_date = datetime.date.today().strftime("%B %d, %Y")

    return templates.TemplateResponse(
        "tasks.html",
        {
            "request": request,
            "tasks": tasks_db,
            "report_date": report_date,
            "progress_percentage": int(progress_percentage),
            "completed_count": completed_count,
            "total_count": total_count,
        }
    )

@app.post("/add_task", response_class=RedirectResponse)
async def add_task(work_item: str = Form(...), description: str = Form(...)):
    """Handle task submission and redirect to the home page."""
    new_id = max(task.id for task in tasks_db) + 1 if tasks_db else 1
    new_task = Task(id=new_id, work_item=work_item, description=description, status="In Progress")
    tasks_db.append(new_task)
    # Redirect to the home page (status code 303 ensures a GET request)
    return RedirectResponse(url="/", status_code=303)

@app.post("/update_status/{task_id}")
async def update_status(task_id: int, status: str = Form(...)):
    """Update the status of a specific task."""
    for task in tasks_db:
        if task.id == task_id:
            task.status = status
            return RedirectResponse(url="/", status_code=303)
    raise HTTPException(status_code=404, detail="Task not found")
