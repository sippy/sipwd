# About

This is a simple watchdog application designed to periodically check the
presence of some daemons and start them if necessary. It has proven very useful
for monitoring complex C programs (e.g., SER, rtpproxy) that may terminate
unexpectedly due to code bugs.

# Usage

The usage is straightforward - `sipwd` should be started periodically (for
example, every minute by `cron(8)`) for each program to be monitored. All
necessary arguments are passed via the command line. The synopsis is as
follows:

```plaintext
sipwd <runfile> <binfile> <pidfile> <command> [<force_restart_file>]
```

- `<runfile>`: Name of the file indicating that the service has been started.
  If it doesn't exist, the watchdog exits.
- `<binfile>`: Name of the binary for the service to be checked. For
  interpreted languages like Python or Perl, this should be the interpreter's
  name.
- `<pidfile>`: Name of the file containing one or more PIDs for the service.
  The watchdog verifies that all PIDs match the running processes.
- `<command>`: Command to start the service if it's not running.
- `<force_restart_file>`: Optional. Used to forcibly restart the monitored
  application if it starts working improperly.

For example, to monitor the rtpproxy service:

```shell
sipwd /var/run/rtpproxy.runs /usr/local/bin/rtpproxy \
/var/run/rtpproxy.pid "/usr/local/etc/rc.d/rtpproxy.sh start"
```

The watchdog uses the `siplog` library for logging. The `SIPLOG_BEND` and
`SIPLOG_LOGFILE_FILE` environment variables can be set to modify its behavior.

## Integration

1. Install `sipwd`.
2. Ensure `procfs` is mounted and enabled in `/etc/fstab`.
3. Modify the startup scripts of services to be monitored to create/remove
   `<runfile>` on start/stop. Example:

    ```shell
    case "$1" in
    start)
      /usr/local/bin/rtpproxy -l 1.2.3.4 -p /var/run/rtpproxy.pid
      touch /var/run/rtpproxy.runs
      ;;
    stop)
      rm -f /var/run/rtpproxy.runs
      /bin/kill -TERM `/bin/cat /var/run/rtpproxy.pid 2>/dev/null` 2>/dev/null
      ;;
    esac
    ```

4. Add the appropriate `crontab(5)` entry. If the machine has multiple
   services to be monitored, it's practical to create a shell script and put
   multiple invocations of `sipwd` into it, then invoke that script from
   `cron(8)`.
