// modbus test framework.
// p. wilshire 
// 09_18_2023

// This is more complete but still needs work.
// 
const express = require('express');
const axios = require('axios');

const { exec } = require('child_process');
const fs = require('fs');  // import the fs module
const path = require('path');  // import the path module

const app = express();
const PORT = 3000;
const DATA_DIR = './data';  // directory where layout data is saved

app.use(express.json());  // middleware to parse JSON requests
app.use(express.static('public'));

// async function fetchData() {
//     const remoteResponse = await axios.get(`http://127.0.0.1:5000/run-command`, {
//         // ... other code
//     });
//     // ... rest of the function
// }

// fetchData();


app.get('/run-command', async (req, res) => {
    const { ip, port, action, deviceId, type, offset, value, rowId } = req.query;
    try {
        // Forward the command to the remote Python server
        const remoteResponse = await axios.get(`http://127.0.0.1:5000/run-command`, {
            params: {
                ip,
                port,
                action,
                deviceId,
                type,
                offset,
                value,
                rowId
            }
        });

        // Send back the response from the remote server to the client
        res.send(remoteResponse.data);
    } catch (error) {
        console.error('Error forwarding command to remote server:', error);
        res.status(500).send({ result: 'fail', message: 'Error forwarding command to remote server' });
    }
    // const cmd = `./test_io ${ip} ${port} ${action} ${deviceId} ${type} ${offset} ${value}`;
    // const rcmd= '{"result":"pass","value":3456}'
    // return res.send(rcmd);
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

    //let formattedData = "{\n    \"rows\": [\n";
    //let formattedData = "{\n    \"title\": " + JSON.stringify(title)|| "Untitled" + ",\n";
    const title = req.body.title || "Untitled 3";
    let formattedData = "{\n    \"title\": \"" + title + "\",\n";

    formattedData += "   \"rows\": [\n";
    req.body.rows.forEach((row, index) => {
        formattedData += "        " + JSON.stringify(row);
        if (index !== req.body.rows.length - 1) {
            formattedData += ",\n";
        }
    });
    formattedData += "\n    ]\n}";

    // let data = JSON.stringify(req.body);
    // // Then replace every '],' with '],\n'
    // data = data.replace("],"/g, "],\n");
    // //data = data.replace(/],/g, "],\n");

    fs.writeFile(filename, formattedData, err => {
        if (err) {
            return res.status(500).send("Error saving layout");
        }
        res.json({ success: true });
    });
});

app.get('/saveLayoutAs', (req, res) => {
    const filename = path.join(DATA_DIR, req.query.filename);
    //let formattedData = "{\n    \"rows\": [\n";
    //let formattedData = "{\n    \"title\": " + JSON.stringify(title) || " Untitled 2" + ",\n";
    const title = req.body.title || "Untitled 2";
    let formattedData = "{\n    \"title\": \"" + title + "\",\n";

    formattedData += "   \"rows\": [\n";

    req.body.rows.forEach((row, index) => {
        formattedData += "        " + JSON.stringify(row);
        if (index !== req.body.rows.length - 1) {
            formattedData += ",\n";
        }
    });
    formattedData += "\n    ]\n}";

    // const data = JSON.stringify(req.body);
    fs.writeFile(filename, formattedData, err => {
        if (err) {
            console.log(`save error: ${err}`);
            return res.status(500).send("Error saving layout");
        }
        res.json({ success: true });
    });
});

app.post('/saveLayoutAs', (req, res) => {
    const filename = path.join(DATA_DIR, req.query.filename);
    const title = req.body.title || "Untitled 1";
    let formattedData = "{\n    \"title\": \"" + title + "\",\n";
    formattedData += "   \"rows\": [\n";
    req.body.rows.forEach((row, index) => {
        formattedData += "        " + JSON.stringify(row);
        if (index !== req.body.rows.length - 1) {
            formattedData += ",\n";
        }
    });
    formattedData += "\n    ]\n}";
    //const data = JSON.stringify(req.body);
    fs.writeFile(filename, formattedData, err => {
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

