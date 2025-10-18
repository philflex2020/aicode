from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, JSONResponse, Response
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
import httpx
import json
import uvicorn
import os

app = FastAPI(title="RBMS Dashboard Frontend")

# URL of the backend data server
DATA_SERVER_URL = os.environ.get("DATA_SERVER_URL", "http://localhost:8081")

# Mount static files
app.mount("/static", StaticFiles(directory="static"), name="static")

# Templates
templates = Jinja2Templates(directory="templates")

def load_dash_config():
    with open("dash.json", "r", encoding="utf-8") as f:
        return json.load(f)

@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.get("/dash.json")
def dash_json():
    return JSONResponse(content=load_dash_config())

# Proxy helper
async def proxy_to_data_server(path: str, request: Request):
    async with httpx.AsyncClient() as client:
        url = f"{DATA_SERVER_URL}{path}"
        if request.method == "GET":
            params = dict(request.query_params)
            resp = await client.get(url, params=params)
        elif request.method == "POST":
            body = await request.body()
            resp = await client.post(url, content=body, headers={"Content-Type": "application/json"})
        else:
            return Response(content="Method not allowed", status_code=405)
        return Response(content=resp.content, status_code=resp.status_code, media_type=resp.headers.get("content-type"))

# Proxied routes
@app.get("/metrics")
async def metrics_proxy(request: Request):
    return await proxy_to_data_server("/metrics", request)

@app.get("/vars")
async def vars_proxy(request: Request):
    return await proxy_to_data_server("/vars", request)

@app.post("/vars")
async def vars_post_proxy(request: Request):
    return await proxy_to_data_server("/vars", request)

@app.get("/series")
async def series_proxy(request: Request):
    return await proxy_to_data_server("/series", request)

@app.get("/api/state")
async def state_get_proxy(request: Request):
    return await proxy_to_data_server("/api/state", request)

@app.post("/api/state")
async def state_post_proxy(request: Request):
    return await proxy_to_data_server("/api/state", request)

@app.post("/api/button/{name}")
async def button_proxy(name: str, request: Request):
    return await proxy_to_data_server(f"/api/button/{name}", request)

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8082, reload=True)
