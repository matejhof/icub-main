#!/bin/bash

source ./config.sh

$EXECUTABLE_PATH/$HISTTRACKER_EXECUTABLE --name $HISTTRACKER_NAME --file $CONFIGURATION_PATH/$HISTTRACKER_CONFIGFILE --group $HISTTRACKER_CONFIGGROUP
