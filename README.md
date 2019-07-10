extech-reader repository
------------------

###This is a collection of programs and library code to read from an Extech 380803 family of power meters

* extech-rdr - the main program, stores readings into a file in binary. very compact.
* extech-powermeter - like having the power meter on your terminal, instead of back in the lab.  can store readings to a file in ascii format, which can later be sorted and whatnot.
* extech-decode - decode readings stored by extech-rdr

* additional shell scripts to help out here and there with sorting and decoding and debugging and other things.

###Known bugs:
* Sometimes the readings doesn't come out right.  This is due to bugs in the code that translates the bits received from the meter into a reading (number).  Since that code was riffed from another program, I never actually went back and fixed the code myself.  But I did get the protocol documentation from Extech, so if I ever felt motivated, I could go back and fix it.  Since this doesn't happen enough to really be too annoying, it gets back-burnered in perpetuity.


