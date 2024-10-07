@echo off
setlocal enabledelayedexpansion

REM Get the arguments passed in the command line

REM Command
set upload_cmd=%1

REM Paths
set mtb_tools_path=%2
set platform_path=%3
set build_path=%4

REM Board parameters 
set board_variant=%5
set board_serial_num=%6
set board_openocd_cfg=%7

REM Project parameters
set project_name=%8

REM "Optional arguments"
set verbose_flag=%9

REM Replace backslashes with forward slashes
REM for openocd to work properly
set "mtb_tools_path=!mtb_tools_path:\=/!"
set "platform_path=!platform_path:\=/!"
set "build_path=!build_path:\=/!"

%mtb_tools_path%/modus-shell/bin/bash -i -l -c "bash %platform_path%/tools/upload.sh %upload_cmd% %mtb_tools_path% %platform_path% %build_path% %board_variant% %board_serial_num% %board_openocd_cfg% %project_name% %verbose_flag%"