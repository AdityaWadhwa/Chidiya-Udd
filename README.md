# Chidiya-Udd
The title of the project “Chidiya udd” describes a popular Indian game which almost each Indian has played in their childhood. 

In the contemporary form, a person becomes a moderator and says the name of many objects which can either fly or they don’t and the players have to raise their finger in accordance with the object. 

This project replicates exactly the contemporary form but eliminating the need of a moderator which is fulfilled by the microcontroller. 
The project is developed under the guidance of Mr. D.V Gadre, CEDT lab in Netaji Subhas Institute of Technology (now, Netaji Subhas University of Technology). 

It was developed alongside my friend Sumiran Bhasin (https://github.com/sumiran14)

The microcontroller chosen is Texas Instruments low power device MSP430G2553.  

This project also interfaces two external SPI devices, the Graphics LCD and a flash memor; a speaker is used for the voice output.
The input from the player  is taken through a capacitive touch interface. 

As of now, it speaks only speaks in one language and only the name of the object. The next versions of the same project will have its own battery dependent power supply, better voice quality and more speed

The need of using external memory (an SPI flash) arised because MSP’s main memory couldn’t hold large amounts of data to play the audio files. 
The audio files are first downloaded (using Google translate) and converted and sampled at 8kHz and stored in 8 bit format(because human voice has maximum frequency of 4 kHZ and we sample it at at least twice that frequency) 
and saved in .wav format. Then using an online software they are easily converted into c files. 
The data in the C files is really large so any microcontroller memory can’t fit all of that. 
So the data is saved on the hard drive of the computer and is sent through a python program to an Arduino and then written in external memory through Arduino.  
When the data has to be played, MSP430 first fetches the data from SPI flash. 
That data decides the duty cycle of the PWM wave.The PWM wave is then sent as an input to a PAM IC and finally to a speaker to hear us the voice. As the data is sampled at 8Khz the duty cycle of the PWM wave has to be changed after every 1/8000th of a second i.e the next data is fetched from the external memory and played for 1/8000th of a second. 

The Graphics LCD is used to display the score. The code for the Graphics LCD is inspired by a blog by Rohit Gupta. 
Since timers are used for generating PWM, developing our own code for capacitive touch sensing was essential. 

In general, capacitive touch works on the basis of an oscillator with a capacitor. 
For a fixed period of time, the number of oscillations in that period would not vary much if the capacitance doesn’t change. 
Due to human interaction, capacitance of the circuit changes drastically and the variation in oscillations in the fixed time are different by a substantial amount causing us to detect a touch. 
MSP430G2553 have built in pin oscillators and by changing some values in registers we can make them enabled and use them as a clock source to one of the timers. 

The overall codeflow is that at the start the player has a fixed score of 9. The game doesn’t start as soon as the finger of the player isn’t on the touchplate and as one loses their score decreases. A random number generator generates a random number which calls a specific object. When the player has score 0 the reset button has to be pressed to play the game again.

Each code and EAGLE files for the same have been attached.
