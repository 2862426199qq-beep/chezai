sudo rmmod fmcw_radar 2>/dev/null
cd /home/cat/chezai/driver/fmcw_radar/try && sudo insmod radar1.ko
echo fmcw_radar | sudo tee /sys/bus/spi/devices/spi0.0/driver_override
echo spi0.0 | sudo tee /sys/bus/spi/drivers/fmcw_radar/bind
sudo chmod 666 /dev/fmcw_radar