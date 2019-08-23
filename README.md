extech-reader repository
------------------

### This is a collection of programs and library code to read from an Extech 380803 family of power meters

* extech\_rdr - the main program: takes an argument of number-of-seconds to run, and outputs the amount of power consumed in watt-hours; can also store readings into a file in a very compact binary; has a MAX option which is similar to the MAX button on the power meter.
* extech-powermeter - like having the power meter on your terminal, instead of back in the lab.  can store readings to a file in ascii format, which can later be sorted and whatnot.
* extech-decode - decode readings stored by extech\_rdr

* various helper programs in the form of shell scripts and C programs to assist here and there with sorting and decoding and debugging and whatnot.

### Known bugs:
* Sometimes the readings don't decode correctly.  This is due to bugs in the code that translates the bits received from the meter into a number.  Since that code was riffed from the PowerTop program, I never actually went back and fixed the code myself.  But I did get the protocol documentation from Extech, so if I ever felt motivated, I could go back and fix it.  Since this doesn't happen enough to really be too annoying, it gets back-burnered in perpetuity.


