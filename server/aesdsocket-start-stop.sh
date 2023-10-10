#! /bin/sh
# reference [1] Writing shell scripts - Lesson 12: Positional Parameters: https://shorturl.at/cfST2
#	    [2] ECEN5713 Week4 lecture slides
case "$1" in
    start)
        echo "Starting aesdsocket as daemon"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
