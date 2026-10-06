# RemoteOps Design Diary

**Module:** IE3090 – Network Programming  
**Registration Number:** IT24103789  
**Project:** RemoteOps – Remote System Monitoring and Management Tool

## Initial Planning and Personalisation

I first reviewed the RemoteOps protocol and calculated all personalised values from my registration number. The Agent port was calculated as 9410, the source files were named `agent_789.c` and `controller_789.c`, the authentication token was set to `OPS-3789`, and the Session ID was set to `SID:9873`. I also configured the personalised log file and storage directory.

I chose a thread-per-client concurrency model using POSIX pthreads because it provides a straightforward way to allow multiple Controllers to communicate with one Agent at the same time. Each accepted TCP socket is passed to a separate detached thread.

## Authentication and Basic TCP Communication

The initial implementation created the TCP Agent and Controller and implemented AUTH and QUIT. Authentication is maintained separately for each connected Controller session. Commands sent before successful authentication are rejected.

During testing, I verified both incorrect and correct authentication tokens and confirmed that every TCP response contains the personalised SID.

## System Monitoring and Process Information

I implemented SYSINFO using Linux `/proc` files. CPU load is read from `/proc/loadavg`, memory information from `/proc/meminfo`, and uptime from `/proc/uptime`.

One issue encountered during development was that SYSINFO initially returned `UNKNOWN_COMMAND`. I found that the command handler had not yet been connected to the main command parser. I added the handler, rebuilt the Agent and tested it successfully.

LISTPROC was then implemented using `popen()` with the Linux `ps` command. The output is converted into the required `OK PROCS ... SID:9873` protocol response.

## Restricted Remote Execution

I implemented EXEC using a strict whitelist containing only DATE, UPTIME, DISKFREE, HOSTNAME and WHOAMI. Commands outside this list return `ERR 002 COMMAND_NOT_ALLOWED`.

During testing, an EXEC command initially returned `UNKNOWN_COMMAND` because an older Agent binary was still running. I stopped the previous process, rebuilt the application and restarted the Agent. The whitelist then worked correctly.

## File Transfer

PUT was implemented using a text header containing the filename and file size, followed by the exact raw file bytes. Uploaded files are stored under `./agentfiles/IT24103789/`.

While compiling the updated Controller, GCC produced a warning about possible `snprintf()` truncation. I changed the implementation to send the command and newline separately instead of constructing a potentially oversized temporary string. The project then compiled cleanly with `-Wall -Wextra`.

GET was implemented using the reverse process. The Agent sends the filename and exact file size before sending the raw bytes. The Controller saves the result as `downloaded_<filename>`.

I tested both PUT and GET using SHA-256 checksums and `cmp`. The original, uploaded and downloaded files were byte-for-byte identical.

## UDP Monitoring

I implemented MONITOR START and MONITOR STOP using a secondary UDP channel. The Controller opens a UDP listener on the requested port, while the Agent periodically sends system information datagrams containing the personalised SID. A five-second monitoring interval was selected to provide regular updates without excessive traffic.

I verified that UDP messages arrived while monitoring was active and stopped after MONITOR STOP.

## Logging and Disconnect Handling

The Agent was extended to write timestamped events to `remoteops_IT24103789.log`. Connections, commands, file transfers and disconnects are recorded. Authentication tokens are redacted from log entries.

Both graceful and unexpected Controller disconnects were tested. After an unexpected disconnect, the Agent remained operational and accepted a new Controller connection.

## Concurrency Testing

Finally, I tested five simultaneous Controller connections. All five authenticated successfully and were able to issue different commands while remaining connected. The number of established TCP connections on port 9410 was verified using `ss`.

This confirmed that the pthread-based concurrency design satisfies the requirement for at least five simultaneous Controllers.

## Current Result

The final implementation successfully supports authentication, system information, process listing, restricted execution, file upload/download, UDP monitoring, logging, clean disconnect handling and concurrent Controller sessions. Git was used throughout development with incremental commits for major implementation stages.
