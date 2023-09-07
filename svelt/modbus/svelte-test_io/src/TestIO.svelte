<!-- TestIO.svelte -->
   <script>
       let ip = '';
       let port = '';
       let action = '';
       let deviceId = '';
       let type = '';
       let offset = '';
       let value = '';
       let result = '';

       async function handleSubmit() {
           // Call backend API or run command here.
           // For this example, I'll make a simple API call.
           const response = await fetch(`/run-command?ip=${ip}&port=${port}&action=${action}&deviceId=${deviceId}&type=${type}&offset=${offset}&value=${value}`);
           result = await response.text();
       }
   </script>

   <div>
       <input bind:value={ip} placeholder="IP" />
       <input bind:value={port} placeholder="Port" />
       <select bind:value={action}>
           <option value="set">Set</option>
           <option value="pub">Publish</option>
           <option value="get">Get</option>
       </select>
       <input bind:value={deviceId} placeholder="Device ID" />
       <select bind:value={type}>
           <option value="Holding">Holding</option>
           <option value="Coil">Coil</option>
       </select>
       <input bind:value={offset} placeholder="Offset" />
       <input bind:value={value} placeholder="Value" />
       <button on:click={handleSubmit}>Submit</button>
   </div>

   <div>
       Result: {result}
   </div>
   