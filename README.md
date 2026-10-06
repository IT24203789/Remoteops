# RemoteOps – Remote System Monitoring and Management Tool

## IE3090 Network Programming

**Registration Number:** IT24103789

RemoteOps is a TCP/IP-based remote system monitoring and management tool implemented entirely in C using the BSD sockets API.

The system consists of two programs:

- **Agent** – server program running on the managed Linux machine.
- **Controller** – client program used by an administrator.

TCP is used for authentication, remote commands, system information, process listing, file upload/download, and session control. UDP is used for periodic system monitoring.

---

## Personalised Configuration

| Item | Value |
|---|---|
| Registration Number | IT24103789 |
| Agent TCP Port | 9410 |
| Agent Source File | agent_789.c |
| Controller Source File | controller_789.c |
| Makefile | Makefile_789 |
| Session ID | SID:9873 |
| Authentication Token | OPS-3789 |
| Log File | remoteops_IT24103789.log |
| Storage Directory | ./agentfiles/IT24103789/ |
| Submission Archive | IE3090_IT24103789.zip |

### Personalisation Calculations

- Numeric part of registration number: `24103789`
- First four digits: `2410`
- Agent port: `7000 + 2410 = 9410`
- Last three digits: `789`
- Last four digits: `3789`
- Reversed last four digits: `9873`
- Authentication token: `OPS-3789`

---

## Architecture

~~text
                  TCP Port 9410
+-------------+  <----------->  +----------------------+
| Controller  |                 | RemoteOps Agent      |
|             |                 |                      |
| AUTH        |                 | Authentication       |
| SYSINFO     |                 | Command Processing   |
| LISTPROC    |                 | File Transfer        |
| EXEC        |                 | Logging              |
| PUT / GET   |                 | pthread Concurrency  |
+-------------+                 +----------------------+
       ^                                  |
       |                                  |
       +--------- UDP Monitoring ---------+
                  SYSINFO Datagrams
~~

The Agent uses a **thread-per-connection concurrency model** using POSIX pthreads.

Each accepted Controller connection is handled by a separate detached thread. This allows several Controllers to communicate with the Agent simultaneously without blocking each other.

The implementation was successfully tested with five simultaneous Controller connections.

---

## Implemented Commands

### AUTH

Command:

~~text
AUTH OPS-3789
~~

Successful response:

~~text
OK AUTHENTICATED SID:9873
~~

Invalid authentication response:

~~text
ERR 001 AUTH_FAILED SID:9873
~~

All other commands require successful authentication before they can be executed.

---

### SYSINFO

Command:

~~text
SYSINFO
~~

The SYSINFO command returns CPU load, memory usage, and system uptime.

Response format:

~~text
OK SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:9873
~~

Linux system information is obtained from:

- `/proc/loadavg`
- `/proc/meminfo`
- `/proc/uptime`

---

### LISTPROC

Command:

~~text
LISTPROC
~~

The command returns a snapshot of currently running processes.

Response format:

~~text
OK PROCS <pid/process>,<pid/process>,... SID:9873
~~

The Agent obtains process information using the Linux `ps` command.

---

### EXEC

The RemoteOps Agent supports only the following fixed command whitelist:

~~text
EXEC DATE
EXEC UPTIME
EXEC DISKFREE
EXEC HOSTNAME
EXEC WHOAMI
~~

Successful response format:

~~text
OK EXEC_RESULT <output> SID:9873
~~

Any command outside the whitelist is rejected.

Example:

~~text
EXEC LS
~~

Response:

~~text
ERR 002 COMMAND_NOT_ALLOWED SID:9873
~~

Arbitrary shell execution is therefore not permitted.

---

### PUT

Controller command:

~~text
PUT <local-file>
~~

The Controller calculates the local file size and sends the following protocol header:

~~text
PUT <filename> <filesize>
~~

The header is immediately followed by exactly `<filesize>` raw file bytes.

Successful response:

~~text
OK FILE_RECEIVED <filename> SID:9873
~~

Uploaded files are stored under the personalised directory:

~~text
./agentfiles/IT24103789/
~~

File integrity was verified using SHA-256 hashes and byte-for-byte comparison.

---

### GET

Controller command:

~~text
GET <filename>
~~

Successful Agent response:

~~text
OK FILE_SEND <filename> <filesize> SID:9873
~~

The response line is immediately followed by exactly `<filesize>` raw file bytes.

The Controller stores the downloaded file using the following naming format:

~~text
downloaded_<filename>
~~

If the requested file does not exist, the Agent returns:

~~text
ERR 005 FILE_NOT_FOUND SID:9873
~~

Downloaded files were verified to be byte-for-byte identical to the original files.

---

### UDP Monitoring

Start monitoring:

~~text
MONITOR START 12000
~~

Successful response:

~~text
OK MONITOR_STARTED SID:9873
~~

The Agent sends periodic UDP system-information datagrams approximately every five seconds.

Datagram format:

~~text
SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:9873
~~

Stop monitoring:

~~text
MONITOR STOP
~~

Successful response:

~~text
OK MONITOR_STOPPED SID:9873
~~

---

### QUIT

Command:

~~text
QUIT
~~

Response:

~~text
OK BYE SID:9873
~~

The Agent closes the Controller session cleanly and stops any active monitoring stream associated with that session.

---

## Logging

The Agent writes timestamped connection, command, file-transfer, monitoring, and disconnect events to:

~~text
remoteops_IT24103789.log
~~

Authentication tokens are not stored in plain text in the log.

The Agent handles both:

- Graceful disconnects using `QUIT`
- Unexpected Controller disconnections

An unexpected Controller disconnect does not terminate the Agent process. The Agent continues accepting new Controller connections.

---

## Build Instructions

### Requirements

- Linux / CentOS
- GCC
- POSIX pthread support
- BSD sockets

Compile the project using:

~~bash
make -f Makefile_789
~~

The build generates:

~~text
agent_789
controller_789
~~

---

## Running the Agent

Run:

~~bash
./agent_789
~~

The Agent listens on TCP port:

~~text
9410
~~

The listening port can be verified using:

~~bash
ss -tlnp | grep 9410
~~

---

## Running the Controller

When the Controller and Agent run on the same machine:

~~bash
./controller_789 127.0.0.1
~~

When connecting to an Agent on another machine:

~~bash
./controller_789 <agent-ip-address>
~~

Authentication must be completed before issuing other commands:

~~text
AUTH OPS-3789
~~

---

## Testing Summary

| Test | Expected Result | Actual Result |
|---|---|---|
| Correct authentication | AUTH accepted | Passed |
| Incorrect authentication | AUTH_FAILED returned | Passed |
| SYSINFO | CPU, memory and uptime returned | Passed |
| LISTPROC | Running process snapshot returned | Passed |
| EXEC whitelist | Allowed commands executed | Passed |
| Invalid EXEC | COMMAND_NOT_ALLOWED returned | Passed |
| PUT | File uploaded successfully | Passed |
| PUT integrity | Uploaded file identical to original | Passed |
| GET | File downloaded successfully | Passed |
| GET integrity | Downloaded file identical to original | Passed |
| Missing GET file | FILE_NOT_FOUND returned | Passed |
| MONITOR START | UDP monitoring started | Passed |
| UDP monitoring stream | Periodic SYSINFO datagrams received | Passed |
| MONITOR STOP | UDP monitoring stopped | Passed |
| Graceful QUIT | Session closed cleanly | Passed |
| Unexpected disconnect | Agent remained operational | Passed |
| Five simultaneous Controllers | All five served successfully | Passed |

---

## Concurrency Model

The Agent uses POSIX pthreads to implement concurrency.

When a new Controller connects, the Agent accepts the connection and creates a separate thread using `pthread_create()`.

Each thread executes the client-handling function independently.

The thread is detached using `pthread_detach()`, allowing its resources to be automatically released after the Controller session ends.

This model was selected because it is simple to implement and allows multiple Controller connections to be handled concurrently.

The implementation was tested with five simultaneous Controller connections and all five successfully authenticated and executed commands.

---

## File Transfer Integrity

PUT and GET transfers use exact byte counts.

The implementation repeatedly calls `send()` and `recv()` until the required number of bytes has been transferred.

The uploaded and downloaded test files were compared using:

~~bash
sha256sum test_789.txt agentfiles/IT24103789/test_789.txt
~~

and:

~~bash
sha256sum test_789.txt downloaded_test_789.txt
~~

Byte-for-byte comparison was also performed using:

~~bash
cmp -s test_789.txt downloaded_test_789.txt
~~

The files matched successfully.

---

## Git Development Process

Development was completed using incremental Git commits for the major implementation stages.

The commit history includes development work for:

- Initial personalised project configuration
- TCP Agent and Controller setup
- Authentication
- pthread-based concurrency
- SYSINFO and process information
- Restricted EXEC command whitelist
- TCP PUT file upload
- TCP GET file download
- UDP monitoring
- Timestamped logging and disconnect handling
- Documentation

The repository commit history provides evidence of the development process.

---

## Security Considerations

The implementation includes the following security measures:

- Mandatory authentication before executing other commands
- Fixed EXEC command whitelist
- Rejection of arbitrary shell commands
- Filename validation
- Path traversal prevention
- Maximum upload file-size checking
- Personalised file-storage directory
- Authentication-token redaction in logs
- Exact byte-count handling for PUT and GET
- Graceful handling of client disconnects
- Continued Agent operation after unexpected Controller disconnects

---

## Project Status

The final RemoteOps implementation supports:

- TCP client/server communication
- Authentication
- System information monitoring
- Process listing
- Restricted command execution
- File upload
- File download
- UDP periodic monitoring
- Timestamped logging
- Graceful and unexpected disconnect handling
- Multiple simultaneous Controller connections

All mandatory features were tested successfully on CentOS using the personalised configuration for registration number `IT24103789`.
