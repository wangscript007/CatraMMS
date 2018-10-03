#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/scripts/*.sh

echo "crontab"
crontab -u mms /opt/catramms/CatraMMS/conf/crontab.txt

echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh stop
/opt/catramms/CatraMMS/scripts/nginx.sh status
/opt/catramms/CatraMMS/scripts/nginx.sh start

echo "api"
/opt/catramms/CatraMMS/scripts/api.sh stop
/opt/catramms/CatraMMS/scripts/api.sh status
/opt/catramms/CatraMMS/scripts/api.sh start

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.sh stop
/opt/catramms/CatraMMS/scripts/mmsEngineService.sh status
/opt/catramms/CatraMMS/scripts/mmsEngineService.sh start

echo "encoder"
/opt/catramms/CatraMMS/scripts/encoder.sh stop
/opt/catramms/CatraMMS/scripts/encoder.sh status
/opt/catramms/CatraMMS/scripts/encoder.sh start

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh stop
/opt/catramms/CatraMMS/scripts/tomcat.sh status
/opt/catramms/CatraMMS/scripts/tomcat.sh start

