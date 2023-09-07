// server.js
   express = require('express');
   const { exec } = require('child_process');
   const app = express();
   const PORT = 3000;

   app.use(express.static('public'));

   app.get('/run-command', (req, res) => {
       const { ip, port, action, deviceId, type, offset, value } = req.query;
       const cmd = `./test_io ${ip} ${port} ${action} ${deviceId} ${type} ${offset} ${value}`;

       exec(cmd, (error, stdout, stderr) => {
           if (error) {
               console.error(`exec error: ${error}`);
               return res.send(error);
           }
           res.send(stdout || stderr);
       });
   });

   app.listen(PORT, () => {
       console.log(`Server is running at http://localhost:${PORT}`);
   });