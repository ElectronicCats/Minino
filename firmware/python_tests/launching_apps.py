import serial
import time
import threading

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(2)

running = True

def reading_task():
    try:
        while running:
            if ser.in_waiting > 0:
                data = ser.readline().decode('utf-8').strip()
                if data:
                    print(data)
                else:
                    time.sleep(0.1)
        print("Reading task finished")
    except KeyboardInterrupt:
        pass

def send_command(command, duration=5):
    ser.write((command + '\n').encode('utf-8'))
    time.sleep(duration)

reading_thread = threading.Thread(target=reading_task)

reading_thread.start()

send_command("launch analyzer")
send_command("exit_app")
send_command("launch spam")
send_command("exit_app")
send_command("launch hid")
send_command("exit_app")
send_command("launch adv")
send_command("exit_app")


running = False
reading_thread.join()

ser.close()