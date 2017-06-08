#!/bin/bash

arm-linux-gnueabihf-gcc -static rndis-service.c core-libraries.c -o rndis-service
cp rndis-service /var/www/html/
