#!/bin/bash
# 
# File:   project.sh.bash
# Author: naima
#
# Created on Jul 30, 2023, 9:52:12 AM
#
#create necessary files 
touch /var/log/heater
chmod 666 /var/log/heater
touch /var/log/status
chmod 666 /var/log/status
touch /var/log/messages
chmod 666 /var/log/messages
#start the thermocouple simulator
chmod +x ./x86_thermocouple_client_main
./x86_thermocouple_client_main 1 config_file.json
#start the client
chmod +x ./tcsimd
./tcsimd