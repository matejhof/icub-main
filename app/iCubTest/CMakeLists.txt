# Copyright (C) 2010 RobotCub Consortium
# Author: Alessandro Scalzo alessandro.scalzo@iit.it
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

set(appname iCubTest)

file(GLOB conf ${CMAKE_CURRENT_SOURCE_DIR}/*.ini)
file(GLOB scripts ${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.template)
file(GLOB velTest ${CMAKE_CURRENT_SOURCE_DIR}/conf/vel_control/*.ini)

yarp_install(FILES ${conf} DESTINATION ${ICUB_CONTEXTS_INSTALL_DIR}/${appname})
yarp_install(FILES ${velTest} DESTINATION ${ICUB_CONTEXTS_INSTALL_DIR}/${appname}/vel_control)
yarp_install(FILES ${scripts} DESTINATION ${ICUB_APPLICATIONS_TEMPLATES_INSTALL_DIR})


