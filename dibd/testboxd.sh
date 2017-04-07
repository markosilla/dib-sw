#!/bin/bash
#
# Init file for Testbox daemon
#
# description: execution environment target test framework daemon handling testing
#			  
# processname: testboxd
# 


prog="TESTBOXD"

TESTBOXD=$TESTBOXDIR/bin/testboxd

test -x "$TESTBOXD" || exit 0

case "$1" in
  start)
    echo -n "Starting $prog:"
	start-stop-daemon --start --quiet --exec $TESTBOXD -- $2 $3
	echo "                                                       [ OK ]"
	;;
  stop)
    echo -n "Stopping $prog:"
    start-stop-daemon --stop --quiet --exec $TESTBOXD
    echo "                                                       [ OK ]"
    ;;
  reload|force-reload)
    start-stop-daemon --stop --quiet --signal 1 --exec $TESTBOXD
    ;;
  restart)
    echo -n "Stopping $prog:"
    start-stop-daemon --stop --quiet --exec $TESTBOXD
    echo "                                                       [ OK ]"
    echo -n "Starting $prog:"
    start-stop-daemon --start --quiet --exec $TESTBOXD -- $2 $3
    echo "                                                       [ OK ]"
    ;;
  *)
    echo "Usage: $0 {start|stop|reload|restart|force-reload}"
    exit 1
esac

exit 0
