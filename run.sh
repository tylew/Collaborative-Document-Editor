#!/bin/sh
# 
set -e

BASE="docker-compose.yml"
DEV="docker-compose.dev.yml"

# Default ports
SERVER_PORT="${SERVER_PORT:-9000}"
CLIENT_PORT="${CLIENT_PORT:-3000}"

show_help() {
    cat << EOF
Usage: $0 [MODE] [OPTIONS]

MODES:
  prod          Run in production mode (default)
  dev           Run in development mode with hot reload

OPTIONS:
  --server-port PORT    Set server port (default: 9000)
  --client-port PORT    Set client port (default: 3000)
  -h, --help           Show this help message

EXAMPLES:
  $0                                  # Run in prod mode with default ports
  $0 dev                              # Run in dev mode with default ports
  $0 --server-port 8080               # Run with server on port 8080
  $0 dev --client-port 3001           # Run dev mode with client on port 3001
  $0 --server-port 8080 --client-port 3001  # Custom ports for both

ENVIRONMENT VARIABLES:
  SERVER_PORT           Server port (default: 9000)
  CLIENT_PORT           Client port (default: 3000)

EOF
    exit 0
}

# Parse arguments
MODE="prod"
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            show_help
            ;;
        dev)
            MODE="dev"
            shift
            ;;
        prod)
            MODE="prod"
            shift
            ;;
        --server-port)
            if [ -z "$2" ] || [ "${2#-}" != "$2" ]; then
                echo "Error: --server-port requires a port number"
                exit 1
            fi
            SERVER_PORT="$2"
            shift 2
            ;;
        --client-port)
            if [ -z "$2" ] || [ "${2#-}" != "$2" ]; then
                echo "Error: --client-port requires a port number"
                exit 1
            fi
            CLIENT_PORT="$2"
            shift 2
            ;;
        *)
            echo "Error: Unknown argument '$1'"
            echo "Run '$0 --help' for usage information"
            exit 1
            ;;
    esac
done

# Export ports for docker-compose
export SERVER_PORT
export CLIENT_PORT

echo "Starting services..."
echo "  Server port: $SERVER_PORT"
echo "  Client port: $CLIENT_PORT"
echo "  Mode: $MODE"
echo ""

if [ "$MODE" = "dev" ]; then
    docker compose -f "$BASE" -f "$DEV" up
else
    docker compose -f "$BASE" up
fi