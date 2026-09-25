import serial
import random
import time

PORT = "COM22"      # заміни на свій COM
BAUD = 115200

ser = serial.Serial(PORT, BAUD)

voltage = 52.0

while True:
    # плавно гуляє приблизно як реальна напруга
    voltage += random.uniform(-0.15, 0.15)
    voltage = max(48.0, min(54.0, voltage))

    message = f"V:{voltage:.2f}\r\n"

    ser.write(message.encode())
    print(message.strip())

    time.sleep(0.5)