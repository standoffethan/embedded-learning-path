# Event-Driven State Machine - Project 001

	A project developed with the intention of learning baremetal embedded programming--without the *magic of Arduino libraries*. This is the first in a series of many embedded implementations, and the following is an complete overview of the project, and the process of designing it.

## What is it?

		I built an Event-Driven State Machine using an Arduino UNO R3 (ATmega328p microcontroller). Of course, that is fancy jargan for a circuit that recognizes events, such as button presses, timeouts, and debouncing. In response, executes Interrupt Service Routines to change between three preset states:
- **OFF**
- **ACTIVE**
- **STANDBY**

![Demonstration of circuit, built within TinkerCAD.](./assets/state-demonstration.mp4)