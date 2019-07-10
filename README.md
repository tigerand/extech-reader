extech-reader repository
------------------

###This is a collection of programs and library code to read from an Extech 380803 family of power meters

* extech-rdr - the main program, stores readings into a file in binary. very compact.
* extech-powermeter - like having the power meter on your terminal, instead of back in the lab.  can store readings to a file in ascii format, which can later be sorted and whatnot.
* extech-decode - decode readings stored by extech-rdr

* couple of shell scripts to help out here and there with sorting and other things.

###Known bugs:
* sometimes the readings doesn't come out right.  this is due to bugs in the code that translates the bits received from the meter into a reading.  since that code was riffed from another program, i never actually went back and fixed the code myself.  but i did get the protocol documentation from Extech, so if i ever felt motivated, i could go back and fix it.  doesn't happen all that often though.


