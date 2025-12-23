#!/bin/bash
# Server Control Script for DSA Search Engine

case "$1" in
  start)
    echo "Starting DSA Search Engine..."
    cd /home/azureuser/DSA-project-search-engine/backend
    nohup ./build/search_engine > build/search.log 2>&1 &
    echo "Backend PID: $!"
    echo "Server started. Access at: http://$(curl -s ifconfig.me):8080"
    echo "View logs: tail -f backend/build/search.log"
    ;;
    
  stop)
    echo "Stopping DSA Search Engine..."
    pkill -f search_engine
    echo "Server stopped."
    ;;
    
  restart)
    $0 stop
    sleep 2
    $0 start
    ;;
    
  status)
    if pgrep -f search_engine > /dev/null; then
      echo "✓ Server is RUNNING"
      ps aux | grep search_engine | grep -v grep | head -1
      echo ""
      echo "API Status:"
      curl -s http://localhost:8080/stats | head -10
    else
      echo "✗ Server is NOT running"
    fi
    ;;
    
  logs)
    tail -f /home/azureuser/DSA-project-search-engine/backend/build/search.log
    ;;
    
  *)
    echo "DSA Search Engine Control"
    echo "Usage: $0 {start|stop|restart|status|logs}"
    echo ""
    echo "Commands:"
    echo "  start   - Start the server in background"
    echo "  stop    - Stop the server"
    echo "  restart - Restart the server"
    echo "  status  - Check if server is running"
    echo "  logs    - View live server logs"
    exit 1
    ;;
esac

exit 0
