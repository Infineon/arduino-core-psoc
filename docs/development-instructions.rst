Development Instructions
=========================

..
   TODOS:
   - Contribution guidelines (move here from Contributing.md)
   - Code conventions
   - ....
   - Tools installation:
      - uncrustify
      - python ?
      - pip ?
      - pre-commit hook?
      - spellchecker
      - ... 
   - Installation the Arduino PSoC6 core
   - Setting up the development environment

.. _env_dev_setup:

Environment setup
------------------

.. note::
   | The development environment setup is (currently) only supported on **Linux** |:penguin:|. 
   | As many tools and scripts are not cross-platform, some of them will not directly work on Windows |:abcd:| or macOS |:apple:|.


0. Obviously |:neutral_face:|, install `Arduino IDE (2.0 or higher) <https://docs.arduino.cc/software/ide-v2/tutorials/getting-started/ide-v2-downloading-and-installing/>`_ or `Arduino CLI (1.0.0 or higher) <https://arduino.github.io/arduino-cli/0.24/installation/>`_.

1. Install the :ref:`Infineon PSoC™ microcontroller <psoc_core_installation>` boards package.

2. Locate in your computer where the Arduino15 packages are installed. Usually:

   ::

      ~/.arduino15/

 .. TODO: I would postpone the Windows part, as we won´t have all the dev tools available there
         c:/Users/%USERNAME%/AppData/local/Arduino15 

3. From the Arduino installation location, change directories

   ::

      cd packages/Infineon-psoc/hardware/psoc

   ..
      TODO: We already have the "Infineon" package (from XMC-for-Arduino). We should go for "infineon" instead of "infineon-psoc". And
      we have to see how that works in Windows, to see what happen when we have "Infineon" and "infineon" in the same path, or they are considered
      the same. 
      That arduino-cli naming will be more consistent with the snake_case naming conventions and non-redundant:
      - arduino-cli compile --fqbn infineon:psoc:cy8ckit_062s2_ai 
      instead of
      - arduino-cli compile -fqbn Infineon-psoc:psoc:cy8ckit_062s2_ai 
      or
      - arduino-cli compile -fqbn Infineon:psoc:cy8ckit_062s2_ai

4. Remove any existing installed "x.y.z" version of the core

   ::

      rm -rf x.y.z

   .. This won´t be needed if the package is NOT installed using the .json package index installation.

5. | Clone this repo in a folder with a semver version name. A orderly practice could be to bump the existing version, and add a suffix to indicate the feature or bugfix.
   | For example, from installed ``1.1.0`` to ``1.2.0-new-feature``:

   ::

      git clone https://github.com/Infineon/arduino-core-psoc.git 1.2.0-new-feature

   You can check if the correct version is installed by running

   ::

      arduino-cli core list

   or in the Arduino IDE, navigate to *Tools > Board > Boards Manager...* and search for *PSoC*.

   .. warning::
      | The name of the repo directory needs to be a valid semver (x.y.z) version number. 
      | For more information about the valid Arduino semver notation, see the `Package index specification - Platforms definitions <https://arduino.github.io/arduino-cli/0.34/package_index_json-specification/#platforms-definitions>`_.

6. From the root directory of the core, run the setup script |:computer:|:

   ::

      bash tools/dev-setup.sh

   .. note::
      Alternatively, you can setup the development environment manually |:wrench:| following the steps in :ref:`Manual setup <dev_manual_setup>`.

7. If using the Arduino IDE, restart the application.

8. Start developing |:tools:|!


.. _dev_manual_setup:

Manual setup 
------------

| This section describes the manual setup of the development environment for the Infineon PSoC™ microcontroller core.
| You can consider this a detailed explanation of the development setup process. 
| Unless you want to know the details |:detective:|, it is easier and recommended to use the automated setup script |:scroll:| in :ref:`Environment setup <env_dev_setup>`.

Installing the ArduinoCore-API
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Locate in your computer where the Arduino15 packages are installed. By default:

   ::

      ~/.arduino15/

   .. TODO: I would postpone the Windows part, as we won´t have all the dev tools available there
            c:/Users/%USERNAME%/AppData/local/Arduino15 

2. Change directories to the following path, where your ``arduino-core-psoc`` repository should be cloned:

   ::

      cd packages/Infineon-psoc/hardware/psoc/<x.y.z>

3. 0. Initialize the ArduinoCore-API submodule

   ::

      git submodule update --init


4. Change directories

   ::

      cd cores/psoc

5. Copy or symlink the ``api`` folder from the ArduinoCore-API submodule

   ::

      ln -s ../extras/arduino-core-api/api .

6. You should see now the ``api`` folder in the ``cores/psoc`` directory.


