# Neo-Dice-Duo
Electronic Dice based on NeoPixel ring and MCU ATtiny 85.

The eletronic dice shows two dices to get a random number between 2 and 12.
For visualization a NeoPixel ring with 24 NeoPixel-LEDs is used.
Additionally a 7-segemnt display shows the result.
During rolling the dices, an animation shows the progress. Dependend on the result,
additional animations are used to emphasize some values, especially when the game
"CATAN" is played. https://www.catan.com/catan<br>
Numbers 6 and 8: Very good values, Number 7: special number. This can be customized
in the code to adapt to various games.

The circuit uses these hardware components:
MCU ATtiny 85, NeoPixel ring 24 LEDs, 7-segment display with I2C-controller (Adafruit),
push button to start animation.

The serial interface is used for uploading the code to the MCU using Urboot
bootloader, and is used for a serial console to enter some commands: 'x' to start
animation, 't' to test the likelihood of all possible numbers and show them on the display.

External libraries used: FastLED, SoftI2CMaster<br>

<img width="640" height="480" alt="Dice" src="https://github.com/user-attachments/assets/292bad00-3598-446f-a269-5408f2428275" />
