#!/bin/bash

source ./config.sh

$EXECUTABLE_PATH/$FACEDETECT_EXECUTABLE --name $FACEDETECT_NAME --file $CONFIGURATION_PATH/$FACEDETECT_CONFIG_FILE --group $FACEDETECT_CONFIG_GROUP

