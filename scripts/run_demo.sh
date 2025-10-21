sudo insmod kernel/build/nxp_simtemp.ko
ls -l /dev/simtemp
# Leer una muestra desde Python:
python3 - << 'PY'
import os, struct
f = os.open("/dev/simtemp", os.O_RDONLY)
data = os.read(f, 16)
ts, temp_mC, flags = struct.unpack("<Q i I", data)
print("ts_ns:", ts, "temp:", temp_mC/1000, "C", "flags:", flags)
PY
sudo rmmod nxp_simtemp
dmesg | tail