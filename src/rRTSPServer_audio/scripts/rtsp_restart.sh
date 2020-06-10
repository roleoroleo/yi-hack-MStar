/home/yi-hack/script # cat rtsp_restart.sh
#!/bin/ash

echo "To run this in the background, do this:"
echo "cd /tmp"
echo "nohup 2>&1 /home/yi-hack/script/rtsp_restart.sh &"

echo "Killing all RTSP related processes"
killall wd_rtsp.sh
killall h264grabber_l
killall h264grabber_h
killall rRTSPServer_l
killall rRTSPServer_h
killall rRTSPServer_audio_h
killall rRTSPServer_audio_l
killall h264_grabber_h.sh
killall h264_grabber_l.sh


echo "Creating H264 fifos"
rm -rf /tmp/h264_low_fifo /tmp/h264_high_fifo
/home/yi-hack/bin/mkfifo /tmp/h264_low_fifo
/home/yi-hack/bin/mkfifo /tmp/h264_high_fifo
chmod 755 /tmp/h264_low_fifo
chmod 755 /tmp/h264_high_fifo


echo "Starting H264 grabbers"
/home/yi-hack/script/h264_grabber_h.sh &
/home/yi-hack/script/h264_grabber_l.sh &

echo "Starting RTSP server"
NR_LEVEL=25 /home/yi-hack/bin/rRTSPServer_audio_trial

