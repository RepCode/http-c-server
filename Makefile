.PHONY	:	clean

BINPATH = ./bin/
SRCPATH = ./src/
INCPATH = ./inc/

CC = 
CC-LOCAL = gcc -pthread -Wall -g
CC-ARM = arm-linux-gnueabihf-gcc -pthread -Wall -DARM_PROGRAM

BIN =
BIN-LOCAL = ./bin/x64
BIN-ARM = ./bin/ArmHf

SRC = ./src
ASSETS = assets
INCLUDES = -Iinc -Ii2cDriver/kmodules
MAIN = ServerApp
ENV_FILE = environment.config

REMOTE_USR = ubuntu
REMOTE_DEPLOY_PATH = /home/ubuntu/server
REMOTE_DRIVER_DEPLOY_PATH = /home/ubuntu/driver

DRIVER_KO_PATH = ./i2cDriver/kmodules/i2cDriver.ko

build-local: CC += $(CC-LOCAL)
build-local: BIN += $(BIN-LOCAL)
build-local: build
	

build-arm: CC += $(CC-ARM)
build-arm: BIN += $(BIN-ARM)
build-arm: build

build:
	mkdir -p $(BIN)
	$(CC) $(INCLUDES) $(SRC)/*.c -o $(BIN)/$(MAIN)
	mkdir -p $(BIN)/$(ASSETS) && cp -r ./$(ASSETS) $(BIN)
	cp ./$(ENV_FILE) $(BIN)

run: BIN += $(BIN-LOCAL)
run:
	$(BIN)/$(MAIN)

network-search:
	nmap -sn 192.168.1.0/24

remote-ip:
	$(info **Type "export REMOTE_IP=%REMOTE_IP_VALUE%" to set the IP of the remote device)
	@echo $$REMOTE_IP

remote-connect:
	$(info **Type "export REMOTE_IP=%REMOTE_IP_VALUE%" to set the IP of the remote device)
	ssh $(REMOTE_USR)@$(REMOTE_IP)

remote-deploy: BIN += $(BIN-ARM)
remote-deploy:
	$(info **Type "export REMOTE_IP=%REMOTE_IP_VALUE%" to set the IP of the remote device)
	scp -r $(BIN)/*  $(REMOTE_USR)@$(REMOTE_IP):$(REMOTE_DEPLOY_PATH)

remote-deploy-driver: 
	scp $(DRIVER_KO_PATH)  $(REMOTE_USR)@$(REMOTE_IP):$(REMOTE_DRIVER_DEPLOY_PATH)

get-running-processes:
	pgrep ServerApp
