FRONTEND_DIR = Frontend
BACKEND_DIR  = Backend

.PHONY:frontend
.PHONY:backend
.PHONY:clean

BUILD = DEBUG

all: frontend backend

frontend:
	cd $(FRONTEND_DIR) && $(MAKE) BUILD=$(BUILD)

backend:
	cd $(BACKEND_DIR)  && $(MAKE) BUILD=$(BUILD)

clean:
	cd $(FRONTEND_DIR) && $(MAKE) clean
	cd $(BACKEND_DIR)  && $(MAKE) clean

