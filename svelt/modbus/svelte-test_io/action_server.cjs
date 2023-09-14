const express = require('express');
const app = express();
const fs = require('fs');

app.use(express.static('action_public')); // Assuming your front-end files are in a 'public' directory

app.get('/api/files', (req, res) => {
    // TODO: Fetch and send the list of files.
});

app.get('/api/actions', (req, res) => {
    // TODO: Fetch and send the list of actions.
});

app.post('/api/save-response', (req, res) => {
    // TODO: Receive the response, save with test id in the name.
});

const PORT = 4000;
app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
