# AI Prompt Log

**Module:** IE3090 – Network Programming  
**Project:** RemoteOps  
**Registration Number:** IT24103789  
**AI Tool Used:** ChatGPT

This log records the substantive AI assistance used during the development of Part 1. All suggested code and commands were tested on my own CentOS environment before being included in the final submission.

---

## Interaction 1 – Project Breakdown and Personalisation

**Prompt / Request:**  
I asked ChatGPT to read the assignment requirements, divide the work into manageable phases, calculate the personalised values for registration number IT24103789, and guide me through the implementation step by step.

**AI Assistance:**  
ChatGPT helped calculate:

- TCP port: 9410
- Source files: agent_789.c and controller_789.c
- Makefile: Makefile_789
- Authentication token: OPS-3789
- Session ID: SID:9873
- Log file: remoteops_IT24103789.log
- Storage directory: ./agentfiles/IT24103789/

**How I Used or Changed the Output:**  
I verified the calculations against the assignment formulas before using them in my source files and documentation.

---

## Interaction 2 – GitHub and Git Workflow

**Prompt / Request:**  
I asked for help connecting the CentOS development environment to GitHub using SSH and for guidance on committing and pushing incremental changes.

**AI Assistance:**  
ChatGPT explained commands such as:

- git status
- git add
- git commit
- git pull --rebase
- git push
- git log

It also explained a non-fast-forward push error and advised using pull with rebase instead of force pushing.

**How I Used or Changed the Output:**  
I followed the Git commands and verified each successful push using both the terminal and GitHub repository.

---

## Interaction 3 – SYSINFO Implementation

**Prompt / Request:**  
SYSINFO initially returned `ERR 003 UNKNOWN_COMMAND SID:9873`. I provided my current Agent source code and asked for help implementing the missing feature.

**AI Assistance:**  
ChatGPT suggested reading Linux system statistics from:

- /proc/loadavg
- /proc/meminfo
- /proc/uptime

and returning them using the required RemoteOps protocol format.

**How I Used or Changed the Output:**  
I added the SYSINFO handler, rebuilt the program, and tested the actual output on CentOS. I also compared the values with Linux system information commands.

---

## Interaction 4 – LISTPROC Implementation

**Prompt / Request:**  
After SYSINFO worked, I asked for guidance to implement LISTPROC.

**AI Assistance:**  
ChatGPT suggested using `popen()` with the Linux `ps` command and formatting the result as an `OK PROCS ... SID:9873` response.

**How I Used or Changed the Output:**  
I integrated the handler into the existing command parser, compiled it, and verified the returned process list against the real `ps` output.

---

## Interaction 5 – Restricted EXEC Implementation

**Prompt / Request:**  
I asked for help implementing the fixed EXEC whitelist required by the assignment.

**AI Assistance:**  
ChatGPT provided guidance for supporting only:

- DATE
- UPTIME
- DISKFREE
- HOSTNAME
- WHOAMI

and rejecting all other commands.

**Problem Encountered:**  
`EXEC UPTIME` initially returned `UNKNOWN_COMMAND`.

**How I Used or Changed the Output:**  
I checked the source code and discovered that an older Agent binary/process was still running. I stopped it, rebuilt the latest source code, restarted the Agent, and confirmed that allowed commands worked while `EXEC LS` returned `COMMAND_NOT_ALLOWED`.

---

## Interaction 6 – PUT File Upload

**Prompt / Request:**  
I asked for help implementing the PUT file-transfer command using the required protocol.

**AI Assistance:**  
ChatGPT explained how to send:

`PUT <filename> <filesize>`

followed by exactly the specified number of raw file bytes.

It also suggested storing uploaded files in:

`./agentfiles/IT24103789/`

**Problem Encountered:**  
GCC reported a possible `snprintf()` truncation warning in the Controller.

**How I Used or Changed the Output:**  
I changed the implementation so that the command and newline were sent separately instead of constructing an oversized temporary string. After the change, the project compiled cleanly.

I verified the upload using SHA-256 and `cmp`, confirming that the uploaded file was byte-for-byte identical to the original.

---

## Interaction 7 – GET File Download

**Prompt / Request:**  
I asked for help implementing GET while preserving the existing PUT implementation.

**AI Assistance:**  
ChatGPT explained the required response:

`OK FILE_SEND <filename> <filesize> SID:9873`

followed by exactly the declared number of raw file bytes.

**How I Used or Changed the Output:**  
I added GET handling to both Agent and Controller, tested a successful download, tested the FILE_NOT_FOUND case, and verified the downloaded file using SHA-256 and byte-for-byte comparison.

---

## Interaction 8 – UDP Monitoring

**Prompt / Request:**  
I asked for help adding MONITOR START and MONITOR STOP using a secondary UDP channel.

**AI Assistance:**  
ChatGPT provided guidance for:

- opening a UDP listener on the Controller
- starting a monitoring thread on the Agent
- sending periodic SYSINFO datagrams
- stopping the monitoring stream cleanly

**How I Used or Changed the Output:**  
I tested `MONITOR START 12000`, confirmed that periodic UDP SYSINFO messages containing SID:9873 arrived, and confirmed that messages stopped after MONITOR STOP.

I used a five-second monitoring interval.

---

## Interaction 9 – Logging and Disconnect Handling

**Prompt / Request:**  
I asked for guidance to satisfy the logging and graceful/ungraceful disconnect requirements.

**AI Assistance:**  
ChatGPT suggested adding timestamped entries for connections, commands, file transfers, monitoring, and disconnects to `remoteops_IT24103789.log`.

**How I Used or Changed the Output:**  
I tested normal QUIT handling and also terminated a Controller unexpectedly using Ctrl+C. The Agent remained running and accepted another connection. I verified both cases in the log file.

Authentication tokens were redacted rather than written in clear text.

---

## Interaction 10 – Concurrency Testing

**Prompt / Request:**  
I asked how to prove that the Agent supports at least five simultaneous Controller connections.

**AI Assistance:**  
ChatGPT suggested opening five Controller sessions and checking the established TCP connections on port 9410 using `ss`.

**How I Used or Changed the Output:**  
I successfully authenticated five Controllers simultaneously, executed commands from different sessions, and confirmed the connection count as five.

---

## Interaction 11 – Documentation Review

**Prompt / Request:**  
I asked ChatGPT to help organize the README, design diary, prompt log, evidence requirements, testing summary, and final submission preparation.

**AI Assistance:**  
ChatGPT helped structure the documentation according to the assignment requirements.

**How I Used or Changed the Output:**  
I checked the documentation against my actual implementation, terminal results, personalised values, Git history, and screenshots before including it in my submission.

---

## Overall AI Use

AI was used as a development assistant for explanation, debugging, code structure suggestions, Git troubleshooting, and documentation organization.

I did not rely on AI output without testing it. Suggested changes were compiled and executed on my own CentOS environment, and several suggestions required debugging or modification before they worked correctly.
