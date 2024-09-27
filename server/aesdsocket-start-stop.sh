#!/bin/sh


DAEMON_PATH="/usr/bin/aesdsocket"  # Updated path

NAME=aesdsocket
PIDFILE=/var/run/$NAME.pid
USER=root  # Adjust if needed

case "$1" in
  start)
    echo "Starting $NAME..."
    start-stop-daemon --start --background \
                      --pidfile $PIDFILE \
                      --make-pidfile \
                      --user $USER \
                      --exec $DAEMON_PATH \
                      -- -d  # Pass the -d option to aesdsocket
    ;;
  stop)
    echo "Stopping $NAME..."
    start-stop-daemon --stop --pidfile $PIDFILE
    ;;
  restart)
    echo "Restarting $NAME..."
    start-stop-daemon --stop --pidfile $PIDFILE
    start-stop-daemon --start --background \
                      --pidfile $PIDFILE \
                      --make-pidfile \
                      --user $USER \
                      --exec $DAEMON_PATH \
                      -- -d 
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac

exit 0