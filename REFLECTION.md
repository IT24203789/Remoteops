# Structured Reflection

**Module:** IE3090 – Network Programming  
**Registration Number:** IT24103789  
**Project:** RemoteOps

During the RemoteOps assignment, I used ChatGPT as an AI support tool mainly for understanding the specification, debugging problems, reviewing code structure, Git troubleshooting, and organizing the documentation. I used AI during the implementation of SYSINFO, LISTPROC, the restricted EXEC command, PUT and GET file transfers, UDP monitoring, logging, and concurrency testing. I also used it to understand some Git errors and to check what evidence should be captured for the final report.

One area where AI was useful was breaking a large networking assignment into smaller implementation stages. This made it easier for me to test each feature before moving to the next one. AI also explained Linux-specific sources such as `/proc/loadavg`, `/proc/meminfo`, and `/proc/uptime`, and helped me understand why TCP file transfers must continue calling `send()` and `recv()` until the exact byte count has been transferred. The guidance about testing file integrity with SHA-256 and `cmp` was also useful because it gave me clear evidence that PUT and GET were working correctly.

However, the AI output was not always immediately correct for my environment. For example, SYSINFO initially returned `UNKNOWN_COMMAND` because the handler had not yet been integrated into the command parser. Later, an EXEC command also returned `UNKNOWN_COMMAND`, but the actual cause was that an older Agent binary was still running. I had to stop the process, rebuild the current source code, and test again. During the PUT implementation, GCC produced an `snprintf()` truncation warning. I changed the suggested approach by sending the command and newline separately, which removed the warning and allowed the code to compile cleanly.

I did not accept every AI response without verification. I compiled each major change on CentOS, tested protocol responses, checked the personalised SID and port, compared process and system information with the operating system, and verified file transfers byte-for-byte. I also checked GitHub after pushing commits.

The assignment improved my understanding of practical network programming. I now understand more clearly how TCP and UDP differ, how application-level protocols must handle framing, how multiple `send()` and `recv()` calls may be required, how pthreads can support concurrent clients, and why authentication, command whitelisting, logging, and input validation are important in remote-management software. The debugging process also helped me become more confident in reading compiler warnings, checking running processes, and testing network behaviour systematically.
