# timer pulse

Uses a linux timer (timer_create) to create pulses on a GPIO pin.

Uses command line arguments to select GPIO pin and period.

Also uses command line arguments to enable skipping or inserting pulses for exception handling testing.

To have better timing characteristics, the binary sets it's priority to the highest level, and it's scheduling as FIFO.  In order to run after compiling, use `sudo setcap "cap_sys_nice=eip" tpulse.bin` to allow the program to change it's priority.

To be done: measure jitter.

