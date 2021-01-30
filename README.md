# Arduino ESP8266 Deurbel PCB

I designed a small PCB that houses a Wemos D1 mini, a relay and some status leds. This PCB was designed to smarten our doorbel. We have a simple push button next to our frontdoor. 
This pushbutton closes the loop and the bell rings. 

This PCB, with it's relay, sits between the bell button and the bell. No need to replace the bell power supply (8VAC or similar). Once the bell button is pressed, the relay is closed and the bell is connected to the bell power supply.
At the same time the button press is relayed to Home Assistant using MQTT messaging.
