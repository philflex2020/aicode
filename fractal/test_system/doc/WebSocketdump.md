Yes, you can decrypt WebSocket traffic using Wireshark (or `tshark`, the command-line version of Wireshark) if you have the appropriate encryption keys. WebSocket traffic, when secured, is typically encrypted as part of the HTTPS (TLS/SSL) protocol. To decrypt this in Wireshark, you need access to the TLS session keys or the private key of the server if the traffic uses static RSA key exchange (less common with modern TLS configurations that favor Diffie-Hellman key exchange for better security).

Here’s how to set up decryption in Wireshark:

1. **Using TLS Session Keys**:
   - Modern browsers like Google Chrome and Firefox can export TLS session keys that can be used by Wireshark to decrypt HTTPS traffic, including WebSockets.
   - Set the environment variable `SSLKEYLOGFILE` to a file path where the browser will write the keys.
     ```
     export SSLKEYLOGFILE=/path/to/sslkeys.log
     ```
   - Start your browser with this environment variable, and then start your session. The browser will write the session keys to this file.
   - In Wireshark, go to `Edit` > `Preferences` > `Protocols` > `TLS`, and set the `(Pre)-Master-Secret log filename` to the path of your `sslkeys.log` file.
   - Now, load your pcap file or start a live capture. Wireshark should be able to use the keys to decrypt the traffic automatically.

2. **Using the Server’s Private Key**:
   - This method requires you to have access to the server's private key and only works if the server uses RSA for key exchange (increasingly uncommon).
   - In Wireshark, navigate to `Edit` > `Preferences` > `Protocols` > `TLS`.
   - Under the `RSA keys list`, provide the IP address and port of the server, the protocol (`http`), and the path to the private key file.
   - Again, load your pcap file or start capturing. If the setup is correct and the server uses RSA for the key exchange, Wireshark should decrypt the traffic.

3. **Considerations**:
   - If the traffic uses Perfect Forward Secrecy (PFS), such as with Ephemeral Diffie-Hellman (DHE or ECDHE), the server’s private key won't help in decryption. You will need the session keys as mentioned.
   - Ensure that your method of obtaining keys complies with both legal restrictions and ethical guidelines.

These methods are quite effective for debugging in development environments or legally monitoring your network traffic where you have the right to access the encryption keys. For production environments or sensitive data, ensure proper authorization and security measures are in place.