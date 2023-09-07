const express = require('express');
const { exec } = require('child_process');
const fs = require('fs');  // import the fs module
const path = require('path');  // import the path module

const app = express();
const PORT = 3000;
const DATA_DIR = './data';  // directory where layout data is saved

app.use(express.json());  // middleware to parse JSON requests
app.use(express.static('public'));

app.get('/run-command', (req, res) => {
    const { ip, port, action, deviceId, type, offset, value } = req.query;
    const cmd = `./test_io ${ip} ${port} ${action} ${deviceId} ${type} ${offset} ${value}`;
    const rcmd= '{"result":"pass","value":3456}'
    return res.send(rcmd);
    // exec(cmd, (error, stdout, stderr) => {
    //     if (error) {
    //         console.error(`exec error: ${error}`);
    //         return res.send(error);
    //     }
    //     res.send(stdout || stderr);
    // });
});

// Endpoint to get a list of all available layouts
app.get('/layouts', (req, res) => {
    fs.readdir(DATA_DIR, (err, files) => {
        if (err) {
            return res.status(500).send("Error reading layouts");
        }
        res.json(files);
    });
});

// Endpoint to load a specific layout
app.get('/loadLayout', (req, res) => {
    const filename = path.join(DATA_DIR, req.query.filename);
    fs.readFile(filename, 'utf-8', (err, data) => {
        if (err) {
            return res.status(500).send("Error reading layout");
        }
        res.json(JSON.parse(data));
    });
});

// Endpoints to save layout data
app.post('/saveLayout', (req, res) => {
    const filename = path.join(DATA_DIR, req.query.filename);
    const data = JSON.stringify(req.body);
    fs.writeFile(filename, data, err => {
        if (err) {
            return res.status(500).send("Error saving layout");
        }
        res.json({ success: true });
    });
});

app.get('/saveLayoutAs', (req, res) => {
    const filename = path.join(DATA_DIR, req.query.filename);
    const data = JSON.stringify(req.body);
    fs.writeFile(filename, data, err => {
        if (err) {
            console.log(`save error: ${err}`);
            return res.status(500).send("Error saving layout");
        }
        res.json({ success: true });
    });
});

app.post('/saveLayoutAs', (req, res) => {
    const filename = path.join(DATA_DIR, req.query.filename);
    const data = JSON.stringify(req.body);
    fs.writeFile(filename, data, err => {
        if (err) {
            console.log(`save error: ${err}`);
            return res.status(500).send("Error saving layout");
        }
        res.json({ success: true });
    });
});

app.listen(PORT, () => {
    console.log(`Server is running at http://localhost:${PORT}`);
});

