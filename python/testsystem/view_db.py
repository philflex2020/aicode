from fastapi import FastAPI, Query
from fastapi.responses import JSONResponse
import sqlite3

app = FastAPI()

DB_PATH = "./test_system.db"

@app.get("/query")
def run_query(sql: str = Query(..., description="SQL query to run")):
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        cursor.execute(sql)
        columns = [desc[0] for desc in cursor.description] if cursor.description else []
        rows = cursor.fetchall()
        conn.close()
        results = [dict(zip(columns, row)) for row in rows]
        return JSONResponse(content={"columns": columns, "rows": results})
    except Exception as e:
        return JSONResponse(content={"error": str(e)}, status_code=400)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8001)

