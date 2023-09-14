const axios = require('axios');

const SERVER_URL = 'http://127.0.0.1:5000/run-command';

// Test the GET endpoint
async function testGetCommand() {
    try {
        const params = {
            ip: '127.0.0.1',
            // ... Add other parameters if needed
        };

        const response = await axios.get(SERVER_URL, { params });
        console.log('GET Response:', response.data);
    } catch (error) {
        console.error('Error on GET:', error.response.data);
    }
}

// Test the POST endpoint
async function testPostCommand() {
    try {
        const actions = [
            {id: "1", delay: 0, ip: "127.0.0.1", func: "somefunc", args: "someargs"},
            {id: "2", delay: 0.2, ip: "127.0.0.1", func: "somefunc", args: "someargs"},
            // ... Add more actions if needed
        ];

        const response = await axios.post(SERVER_URL, { actions });
        console.log('POST Response:', response.data);
    } catch (error) {
        console.error('Error on POST:', error.response.data);
    }
}

async function runTests() {
    await testGetCommand();
    await testPostCommand();
}

runTests();
