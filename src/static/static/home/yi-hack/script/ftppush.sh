#!/bin/ash
#
# Command line:
# 	ash "/home/yi-hack/script/ftppush.sh" cron
# 	ash "/home/yi-hack/script/ftppush.sh" start
# 	ash "/home/yi-hack/script/ftppush.sh" stop
#
CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"
YI_PREFIX="/home/app"

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}
# Setup env.
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack/lib:/tmp/sd/yi-hack/lib
export PATH=$PATH:/home/base/tools:/home/yi-hack/bin:/home/yi-hack/sbin:/home/yi-hack/usr/bin:/home/yi-hack/usr/sbin:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin
#
# Script Configuration.
FOLDER_TO_WATCH="/tmp/sd/record"
FOLDER_MINDEPTH="1"
FILE_DELETE_AFTER_UPLOAD="1"
FILE_WATCH_PATTERN="*.mp4"
SKIP_UPLOAD_TO_FTP="0"
SLEEP_CYCLE_SECONDS="45"
#
# Runtime Variables.
SCRIPT_FULLFN="ftppush.sh"
SCRIPT_NAME="ftppush"
LOGFILE="/tmp/${SCRIPT_NAME}.log"
LOG_MAX_LINES="200"
#
# -----------------------------------------------------
# -------------- START OF FUNCTION BLOCK --------------
# -----------------------------------------------------
checkFiles ()
{
	logAdd "[INFO] checkFiles"
	#
	# Search for new files.
	if [ -f "/usr/bin/sort" ]; then
		# Default: Optimized for busybox
		L_FILE_LIST="$(find "${FOLDER_TO_WATCH}" -mindepth ${FOLDER_MINDEPTH} -type f \( -name "${FILE_WATCH_PATTERN}" \) | sort -k 1 -n)"
	else
		# Alternative: Unsorted output
		L_FILE_LIST="$(find "${FOLDER_TO_WATCH}" -mindepth ${FOLDER_MINDEPTH} -type f \( -name "${FILE_WATCH_PATTERN}" \))"
	fi
	if [ -z "${L_FILE_LIST}" ]; then
		return 0
	fi
	#
	echo "${L_FILE_LIST}" | while read file; do
		if ( ! uploadToFtp -- "${file}" ); then
			logAdd "[ERROR] checkFiles: uploadToFtp FAILED - [${file}]."
			continue
		fi
		logAdd "[INFO] checkFiles: uploadToFtp SUCCEEDED - [${file}]."
		if [ "${FILE_DELETE_AFTER_UPLOAD}" = "1" ]; then
			rm -f "${file}"
		fi
		#
	done
	#
	# Delete empty sub directories
	if [ ! -z "${FOLDER_TO_WATCH}" ]; then
		find "${FOLDER_TO_WATCH}/" -mindepth 1 -type d -empty -delete
	fi
	#
	return 0
}


lbasename ()
{
	echo "${1}" | sed "s/.*\///"
}


logAdd ()
{
	TMP_DATETIME="$(date '+%Y-%m-%d [%H-%M-%S]')"
	TMP_LOGSTREAM="$(tail -n ${LOG_MAX_LINES} ${LOGFILE} 2>/dev/null)"
	echo "${TMP_LOGSTREAM}" > "$LOGFILE"
	echo "${TMP_DATETIME} $*" >> "${LOGFILE}"
	echo "${TMP_DATETIME} $*"
	return 0
}


lstat ()
{
	if [ -d "${1}" ]; then
		ls -a -l -td "${1}" | awk '{k=0;for(i=0;i<=8;i++)k+=((substr($1,i+2,1)~/[rwx]/) \
				 *2^(8-i));if(k)printf("%0o ",k);print}' | \
				 cut -d " " -f 1
	else
		ls -a -l "${1}" | awk '{k=0;for(i=0;i<=8;i++)k+=((substr($1,i+2,1)~/[rwx]/) \
				 *2^(8-i));if(k)printf("%0o ",k);print}' | \
				 cut -d " " -f 1
	fi
}


uploadToFtp ()
{
	#
	# Usage:			uploadToFtp -- "[FULLFN]"
	# Example:			uploadToFtp -- "/tmp/test.txt"
	# Purpose:
	# 	Uploads file to FTP
	#
	# Returns:
	# 	"0" on SUCCESS
	# 	"1" on FAILURE
	#
	# Consts.
	FTP_HOST="$(get_config FTP_HOST)"
	FTP_DIR="$(get_config FTP_DIR)"
	FTP_USERNAME="$(get_config FTP_USERNAME)"
	FTP_PASSWORD="$(get_config FTP_PASSWORD)"
	#
	# Variables.
	UTF_FULLFN="${2}"
	#
	if [ "${SKIP_UPLOAD_TO_FTP}" = "1" ]; then
		logAdd "[INFO] uploadToFtp skipped due to SKIP_UPLOAD_TO_FTP == 1."
		return 1
	fi
	#
	if [ ! -z "${FTP_DIR}" ]; then
		# Create directory on FTP server
		echo -e "USER ${FTP_USERNAME}\r\nPASS ${FTP_PASSWORD}\r\nmkd ${FTP_DIR}\r\nquit\r\n" | nc -w 5 ${FTP_HOST} 21 | grep "${FTP_DIR}"
		FTP_DIR="${FTP_DIR}/"
	fi
	#
	if [ ! -f "${UTF_FULLFN}" ]; then
		echo "[ERROR] uploadToFtp: File not found."
		return 1
	fi
	#
	if ( ! ftpput -u "${FTP_USERNAME}" -p "${FTP_PASSWORD}" "${FTP_HOST}" "/${FTP_DIR}$(lbasename "${UTF_FULLFN}")" "${UTF_FULLFN}" ); then
		echo "[ERROR] uploadToFtp: ftpput FAILED."
		return 1
	fi
	#
	# Return SUCCESS.
	return 0
}


serviceMain ()
{
	#
	# Usage:		serviceMain	[--one-shot]
	# Called By:	MAIN
	#
	logAdd "[INFO] === SERVICE START ==="
	# sleep 10
	while (true); do
		# Check if folder exists.
		if [ ! -d "${FOLDER_TO_WATCH}" ]; then 
			mkdir -p "${FOLDER_TO_WATCH}"
		fi
		# 
		# Ensure correct file permissions.
		if ( ! lstat "${FOLDER_TO_WATCH}/" | grep -q "^755$" ); then
			logAdd "[WARN] Adjusting folder permissions to 0755 ..."
			chmod -R 0755 "${FOLDER_TO_WATCH}"
		fi
		#
		if [[ $(get_config FTP_UPLOAD) == "yes" ]] ; then
			checkFiles
		fi
		#
		if [ "${1}" = "--one-shot" ]; then
			break
		fi
		#
		sleep ${SLEEP_CYCLE_SECONDS}
	done
	return 0
}
# ---------------------------------------------------
# -------------- END OF FUNCTION BLOCK --------------
# ---------------------------------------------------
#
# set +m
trap "" SIGHUP
#
if [ "${1}" = "cron" ]; then
	serviceMain --one-shot
	logAdd "[INFO] === SERVICE STOPPED ==="
	exit 0
elif [ "${1}" = "start" ]; then
	serviceMain &
	#
	# Wait for kill -INT.
	wait
	exit 0
elif [ "${1}" = "stop" ]; then
	ps w | grep -v grep | grep "ash ${0}" | sed 's/ \+/|/g' | sed 's/^|//' | cut -d '|' -f 1 | grep -v "^$$" | while read pidhandle; do
		echo "[INFO] Terminating old service instance [${pidhandle}] ..."
		kill -9 "${pidhandle}"
	done
	#
	# Check if parts of the service are still running.
	if [ "$(ps w | grep -v grep | grep "ash ${0}" | sed 's/ \+/|/g' | sed 's/^|//' | cut -d '|' -f 1 | grep -v "^$$" | grep -c "^")" -gt 1 ]; then
		logAdd "[ERROR] === SERVICE FAILED TO STOP ==="
		exit 99
	fi
	logAdd "[INFO] === SERVICE STOPPED ==="
	exit 0
fi
#
logAdd "[ERROR] Parameter #1 missing."
logAdd "[INFO] Usage: ${SCRIPT_FULLFN} {cron|start|stop}"
exit 99
