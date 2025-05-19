FRONTEND_DIR = Frontend
BACKEND_DIR  = Backend

.PHONY:frontend backend clean compile

BUILD = DEBUG

all: frontend backend

frontend:
	cd $(FRONTEND_DIR) && $(MAKE) BUILD=$(BUILD)

backend:
	cd $(BACKEND_DIR)  && $(MAKE) BUILD=$(BUILD)

FILE=test
compile:
	nasm -felf64 $(FILE).asm -o $(FILE).o
	ld $(FILE).o -o $(FILE).exe

clean:
	cd $(FRONTEND_DIR) && $(MAKE) clean
	cd $(BACKEND_DIR)  && $(MAKE) clean

