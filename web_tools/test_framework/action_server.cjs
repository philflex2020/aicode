// This is not really finished . I can do a little more if needed.
// p. Wilshire 09_19_2023
// this lives in the root dir and servers up index.html from action_public
//the test dir defines the tests organized by systems.
// the other app on port 5000 show how to pull in all the json objects 
// 
const express = require('express');
const app = express();
const fs = require('fs');
const path = require('path');


app.use(express.static('action_public')); // Assuming your front-end files are in an 'action_public' directory

app.get('/api/files', (req, res) => {
    // TODO: Fetch and send the list of files.
});

app.get('/api/actions', (req, res) => {
    // TODO: Fetch and send the list of actions.
});

app.post('/api/save-response', (req, res) => {
    // TODO: Receive the response, save with test id in the name.
});

// Endpoint to serve the systems.json file
app.get('/getSystemsData', (req, res) => {
    res.sendFile(path.join(__dirname, './test/systems.json'));
});
// Endpoint to serve the systems.json file
app.get('/getTopicsData', (req, res) => {
    const sysname = req.query.sysname
    res.sendFile(path.join(__dirname, './test/systems', sysname,'topics.json'));
});


const PORT = 4000;
app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
