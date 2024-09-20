Development Instructions
===========================

1. Clone the `ArduinoCore-API <https://github.com/arduino/ArduinoCore-API>`_ repo to any convenient location::

   git clone https://github.com/arduino/ArduinoCore-API.git

2. Locate in your computer where the Arduino15 packages are installed. 
   e.g. Windows: c:/Users/%USERNAME%/AppData/local/Arduino15
        Linux:   ~/.arduino15/

3. Change directories::

   cd packages/Infineon-psoc/hardware/psoc/0.0.1

4. Clone this repo::

   git clone https://github.com/Infineon/arduino-core-psoc.git

5. Change directories::

   cd cores/psoc

6. Copy or symlink the ``api`` folder from the `ArduinoCore-API <https://github.com/arduino/ArduinoCore-API>`_ repo::

   ln -s <ARDUINO_CORE_API>/api .

   where ``<ARDUINO_CORE_API>`` is the location where you've cloned the ArduinoCore-API repository to.

7. Restart the IDE.
