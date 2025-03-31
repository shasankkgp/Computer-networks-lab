# CLDP (Custom Lightweight Discovery Protocol)

## Overview
The CLDP (Custom Lightweight Discovery Protocol) is a simple network protocol designed for querying and retrieving system metadata from active nodes in a network. The protocol is implemented using a client-server model, where the server announces its presence, and the client queries available servers for system-related information.

## Features
- Three message types:
  1. **HELLO (0x01)** - Sent by the server to announce its availability.
  2. **QUERY (0x02)** - Sent by the client to request metadata.
  3. **RESPONSE (0x03)** - Sent by the server in response to a query.
- Custom message headers containing relevant metadata.
- Application-level performance counters:
  - Hostname
  - CPU Load
  - Memory Usage
- Simple UDP-based communication model.

## Message Structure
Each message consists of:
- **Custom Header:** Contains essential metadata fields such as message type, transaction ID, checksum, etc.
- **IP Header:** Standard network layer header.
- **Payload:** Contains hostname and system performance data.

### Custom Header Fields
| Field           | Size (bytes) |
|----------------|-------------|
| Message Type   | 4           |
| Payload Length | 2           |
| Transaction ID | 4           |
| Reserved       | 1           |
| Checksum       | 2           |
| System Time    | 4           |
| Hostname Length| 2           |
| CPU Load       | 4           |
| Memory Usage   | 4           |

## Compilation and Execution
### Compile the Server and Client
```sh
gcc -o server cldp_server.c
gcc -o client cldp_client.c
```

### Running the Server
```sh
sudo ./server
```
#### Expected Server Output:
```
Server IP: 172.17.0.2
Server hostname: ff1482afaff4
Server running... (Press Ctrl+C to stop)
Hello message sent. Transaction ID: 1
Received QUERY from ff1482afaff4 (172.17.0.2)
  Transaction ID: 1
Response sent to ff1482afaff4 (172.17.0.2)
  Transaction ID: 0
Hello message sent. Transaction ID: 2
^C
Shutting down server...
```

### Running the Client
```sh
sudo ./client
```
#### Expected Client Output:
```
Client IP: 172.17.0.2
Client hostname: ff1482afaff4
Query message sent. Transaction ID: 1
Waiting for responses for 5 seconds...

Received RESPONSE from ff1482afaff4 (172.17.0.2)
  Transaction ID: 0
  Remote system time: Mon Mar 31 15:32:05 2025
  CPU Load: 0.00
  Memory Usage: 30.48%
  Time difference: 0.00 seconds
Finished waiting for responses.
Closing connection...
```

## Protocol Workflow
1. The **server** periodically broadcasts a HELLO message to indicate its availability.
2. The **client** sends a QUERY message requesting metadata from available servers.
3. The **server** responds with a RESPONSE message containing:
   - Hostname
   - System time
   - CPU Load
   - Memory Usage
4. The **client** collects and displays the responses.

## Notes
- The server remains active until manually stopped (Ctrl+C).
- The client waits for responses for a fixed duration (e.g., 5 seconds).
- The message structure ensures efficient transmission of metadata over the network.

## Future Enhancements
- Implement additional performance metrics.
- Support for multiple clients and servers in larger networks.
- Secure communication using authentication mechanisms.

## Authors
- G Sai Shasank


