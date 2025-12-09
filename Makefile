.PHONY: run-server run-client stop

run-server:
	@echo "Starting server in Docker..."
	@cd server && make run-preview 

stop-server:
	@echo "Stopping server..."
	@docker stop crdt-server-container

run-client:
	@echo "Building client..."
	@cd client && npm run build
	@echo "Starting client preview in background..."
	@cd client && nohup npx vite preview --host --port 4173 --strictPort >/dev/null 2>&1 & echo $$! > .vite.pid
	@echo "Client running at http://localhost:4173"

start: run-server run-client
	@echo "Setting up Tailscale funnel..."
	@tailscale funnel --https=443 --set-path=/ --bg http://127.0.0.1:4173
	@tailscale funnel --https=443 --set-path=/ws --bg http://127.0.0.1:9000
	@echo "System ready and serving to public internet."

	@echo Attaching to server...
	@docker attach crdt-server-container

stop:
	@echo "Stopping services..."
	@-docker stop crdt-server-container || true
	@-pkill -f "vite preview" || true
	@-echo y | tailscale funnel --https=443 off || true