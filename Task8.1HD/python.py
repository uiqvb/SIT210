import speech_recognition as sr
import asyncio
from bleak import BleakClient

DEVICE_ADDRESS = "B0:B2:1C:56:AB:82"
CHAR_UUID = "19b10001-e8f2-537e-4f6c-d104768a1214"

async def send_command(command):
    async with BleakClient(DEVICE_ADDRESS, timeout=20.0) as client:
        await client.write_gatt_char(CHAR_UUID, command.encode(), response=False)
        print(f"Sent: {command}")

def listen_for_command():
    recognizer = sr.Recognizer()
    with sr.Microphone() as source:
        print("Listening...")
        recognizer.adjust_for_ambient_noise(source, duration=0.2)
        audio = recognizer.listen(source, timeout=10, phrase_time_limit=2)
    try:
        text = recognizer.recognize_google(audio).lower()
        print(f"You said: {text}")
        return text
    except:
        return ""

async def main():
    print("Voice control ready!")
    while True:
        text = listen_for_command()
        if "lights on" in text:
            await send_command("LIGHTS_ON")
        elif "lights off" in text:
            await send_command("LIGHTS_OFF")

asyncio.run(main())