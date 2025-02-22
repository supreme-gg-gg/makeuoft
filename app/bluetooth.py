import serial
import struct
import time
import cv2
import numpy as np
import threading

# Adjust the serial port and baud rate as needed.
SERIAL_PORT = '/dev/rfcomm0'
BAUD_RATE = 115200

# Global flag to signal exit
exit_flag = False

def send_servo_command(ser, angle1, angle2):
    """Send a servo command to set the servos to the given angles.
       The command format is: "CMD:<angle1>,<angle2>\n"
    """
    if not ser.is_open:
        try:
            ser.open()
        except Exception as e:
            print("Failed to open port before writing:", e)
            return
    command = f"CMD:{angle1},{angle2}\n"
    try:
        ser.write(command.encode())
        print(f"Sent servo command: {command.strip()}")
    except Exception as e:
        print("Error writing to port:", e)

def read_frame(ser):
    """Read a single video frame from the serial port."""
    if not ser.is_open:
        try:
            ser.open()
        except Exception as e:
            print("Failed to open port before reading:", e)
            return None

    header = ser.read(4)
    if len(header) < 4:
        print("Incomplete header received")
        return None

    frame_size = struct.unpack('<I', header)[0]
    data = ser.read(frame_size)
    if len(data) < frame_size:
        print("Incomplete frame received")
        return None
    return data

def servo_input_thread(ser):
    """Thread that reads user input from the terminal and sends servo commands.
       Expected input: two comma-separated angle values, e.g., "90,45".
    """
    global exit_flag
    while not exit_flag:
        user_input = input("Enter servo angles (0-180, comma separated) or 'q' to quit: ")
        if user_input.lower() == 'q':
            exit_flag = True
            break

        if ',' not in user_input:
            print("Invalid format. Please enter two angles separated by a comma (e.g., 90,45).")
            continue

        parts = user_input.split(',')
        if len(parts) != 2:
            print("Please provide exactly two values separated by a comma.")
            continue

        try:
            angle1 = int(parts[0].strip())
            angle2 = int(parts[1].strip())
            if 0 <= angle1 <= 180 and 0 <= angle2 <= 180:
                send_servo_command(ser, angle1, angle2)
            else:
                print("Both angles must be between 0 and 180.")
        except ValueError:
            print("Invalid input. Please enter numeric values for the angles.")

def main():
    global exit_flag
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)
    except Exception as e:
        print(f"Failed to open {SERIAL_PORT}: {e}")
        return

    # Start the servo input thread.
    input_thread = threading.Thread(target=servo_input_thread, args=(ser,), daemon=True)
    input_thread.start()

    print("Starting video stream. Press 'q' in the video window or enter 'q' in the terminal to quit.")

    try:
        while not exit_flag:
            frame_data = read_frame(ser)
            if frame_data is None:
                # If frame reading fails, wait a bit and try again.
                time.sleep(0.1)
                continue

            # Convert the JPEG data to a NumPy array
            np_arr = np.frombuffer(frame_data, dtype=np.uint8)
            # Decode the image from the NumPy array
            frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            if frame is None:
                print("Failed to decode frame")
                continue

            # Optionally, print out the resolution
            height, width = frame.shape[:2]
            print(f"Frame resolution: {width} x {height}")

            # Display the frame in a window
            cv2.imshow("Live Stream", frame)
            # Wait 1ms for key event. Press 'q' in the video window to quit.
            if cv2.waitKey(1) & 0xFF == ord('q'):
                exit_flag = True
                break

    except KeyboardInterrupt:
        print("Keyboard interrupt received, exiting...")
    finally:
        exit_flag = True
        ser.close()
        cv2.destroyAllWindows()
        input_thread.join()

if __name__ == '__main__':
    main()
