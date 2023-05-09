# Aegis-2
The Aegis 2 Handheld with a 7" screen

NOTE: This is still a work in progress.

## Summary

The Aegis 2 is a DIY handheld that uses a 3D printed case with a Raspberry Pi CM4 and 1920x1200 7" screen.

## Building the Handheld

To build the handheld you will need the items listed as required in the Bill of Materials along with the following tools:

Reflow Station or Reflow Oven<br/>
Solding Iron<br/>
Solder Paste<br/>
Tweezers<br/>
Small Philips Screw Driver<br/>
Small Flathead Screw Driver<br/>
Super Glue (preferrably CA glue with Accelerator)

There are items in the Bill of Materials that are listed as optional. These items are not required to get a working device but can be used if desired. At this time I will not provide directions on how to use the optional components so use at your discretion.

### Assembling the Circuit Board

The custom circuit boards are provided as fusion 360 files in this repository. This includes the schematic, board layout, and the gerber files. 

To get the boards printed, a manufacturing facility is required. The website jlcpcb.com was used to print the 3 circuit board prototypes for the design. The larger of the two boards uses 4 layers, and the smaller ones use only 2 layers. It is recommended to also get the stencils with the boards so that applying the solder paste becomes much easier.

Once the boards have been received you will need to assemble the circuitry on the boards. This is done by getting all of the components provided in the bill of materials. Each item has a provided Link to where the item can be acquired along with the price at the time of the assembly of the prototype. After acquiring all of the components, use the stencil to apply solder paste to the circuit boards and then use tweezers to place each component. A reflow oven or reflow station should then be used to melt the solder paste and adhere the components. Lastly, any through hole components were then added on the using a soldering iron. 

Note: The teensy that is soldered to the board using the through holes. However, there are 4 pads that are located near the USB connector of the Teensy on the custom circuit board. They are the VBUS, D+, D-, and GND connections. I included the VBUS and GND connections as backups but they are not needed as the power and ground comes from the pins. But the D+, D- connections need to be connected to the teensy. On the underside of the teensy there are 4 pads that are in about the same location, a wire or some other form of connection is needed to connect the D+ from the teensy to the D+ on the custom circuit board, do the same with the D- connection.

While anyone who has experience with circuit boards will have their own method for assembling these boards I used the following steps and it is what I recommend for anyone without experience and a reflow oven (but a reflow station is required):
1. Using the stencil, put solder paste on the larger of the two boards on the side with a majority of the components
2. Place all of the surface mount components on that side of the board
3. Use a reflow station and heat the underside of the board until the solder paste on the top side melts to adhere the components. A tool to prop up the PCB comes in handy here. In my opinion this part is essential as some of the components are plastic and the hot air coming out of a reflow station can melt them.
4. Use a soldering iron to clean up an connections that didn't connect completely (You can watch youtube videos on how to do this, just search for solding iron and surface mount components)
5. Flip the board over and redo steps 1-4 for the underside of the board. Since these components are larger I just used the a soldering iron for all of the components, but the same procedure with the reflow station can be used. Just be careful with the hot air hitting components you have already put down, as they can fall off if the solder paste melts again.
6. Perform 1-5 for the second board and the few surface mount components that are on that board
7. Add all of the through hole items to both boards with a soldering iron (again youtube is your friend)

### Install Teensy Code

Before (or after) the circuit board has been assembled the teensy code needs to be pushed to the teensy. The teensy code is provided in the repo and you will need to follow the instructions on the teensy website for pushing code. Make sure that you set the board type to "XInput" otherwise it won't be seen as a XBox 360 Controller.

### 3D Printed Case

The files provided here for the case. They are printed in sections so that a printer with a standard bed size of 220mm x 220mm can be used print the case. Once they are printed they will have to be cleaned up and then glued together. Depending on which angle you choose to print the parts the cleanup step could be more or less cumbersome for the remove of support and getting good glue joints. I choose to print everything with flat surfaces on the bed to keep the post processing steps as easy as possible.

Once everything is glued together then no post processing on the outside is technically required to assemble the device. However, I chose to use filler, sand paper, and paint to get a really clean looking finish. The level of detail you desire on your handheld will be up to you based on how much effort you want to devote. Refer to youtube for how to sand and paint a 3D printed piece. However, try to keep the inside of the device as paint free as possible as the internal structure is already at the correct dimensions for the internal components.

I would also sand down the holes where the buttons will be located as they will to be rough from the 3D printing process. This can cause the buttons to stick. I would also check the shoulder buttons to make sure there is not any friction preventing them from moving properly. If there is then sand down the appropriate areas.

### Assembly

Once the case and the PCB have been built and are ready to go the handheld can be assembled.

TODO: Assembly Instructions

## Software

The Raspberry PI OS Buster version or Retropie both work. To get Bullseye working we need a new video driver that is not available yet. I've made a temporary one and will provide it here when I test it out.

The Config files provided that setup the device will install a monitor program that looks for the button presses for the power/reset/volume/misc. And like the previous handheld I built it will also eventually provide an overlay that will show battery/wifi status along with volume/brightness. However, the overlay does not work yet so that part of the code is commented out. But the monitor script does work for the buttons.

### /boot/config.txt

Regardless of which CM4 you use and which method is used to load the OS, you will need to add the following lines to the /boot/config.txt file. These are also listed in the /Config/boot/config.txt file in this repository:

dtoverlay=dwc2,dr_mode=host<br/>
h264_freq=333

The lines above will optimize the graphics for Moonlight (if you choose to use it to stream PC games) and will enable USB 2.0 on the device to get the Teensy and external USB ports working.

### First Boot

TODO

### System Setup

TODO

## Pictures/Videos

TODO

## Credits

Safe shutdown script
https://github.com/crcerror/ES-generic-shutdown/blob/master/multi_switch.sh
