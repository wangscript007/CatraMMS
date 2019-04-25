#!/bin/bash

#update certificate
#sudo /usr/bin/certbot --quiet renew --pre-hook "/opt/catramms/CatraMMS/scripts/nginx.sh stop" --post-hook "/opt/catramms/CatraMMS/scripts/nginx.sh start"

#Retention (3 days: 4320 mins, 1 day: 1440 mins, 12 ore: 720 mins)

if [ $# -ne 1 ]
then
    echo "$(date): usage $0 commandIndex" >> /tmp/crontab.log

    exit
fi

commandIndex=$1

if [ $commandIndex -eq 1 ]
then
	commandToBeEecuted="find /var/catramms/logs -mmin +4320 -type f -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 2 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSGUI/temporaryPushUploads -mmin +4320 -type f -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 3 ]
then
	commandToBeEecuted="find /var/catramms/storage/IngestionRepository/ -mmin +4320 -type f -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 4 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSWorkingAreaRepository/ -mmin +720 -type f -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 5 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg -mmin +4320 -type f -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 6 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging -mmin +60 -type f -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 7 ]
then
	commandToBeEecuted="find /var/catramms/storage/DownloadRepository/* -empty -mmin +4320 -type d -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 8 ]
then
	commandToBeEecuted="find /var/catramms/storage/StreamingRepository/* -empty -mmin +4320 -type d -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 9 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSRepository/MMS_????/*/* -empty -mmin +4320 -type d -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 10 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSWorkingAreaRepository/Staging/* -empty -mmin +720 -type d -delete"
	timeoutValue="1h"
elif [ $commandIndex -eq 11 ]
then
	commandToBeEecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/* -empty -mmin +720 -type d -delete"
	timeoutValue="1h"
else
	echo "$(date): wrong commandIndex: $commandIndex" >> /tmp/crontab.log

	exit
fi

timeout $timeoutValue $commandToBeEecuted
if [ $? -eq 124 ]
then
	echo "$(date): $commandToBeEecuted TIMED OUT" >> /tmp/crontab.log
elif [ $? -eq 126 ]
then
	echo "$(date): $commandToBeEecuted FAILED (Argument list too long)" >> /tmp/crontab.log
fi

