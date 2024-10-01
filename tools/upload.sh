#!/bin/bash

# Command
upload_cmd=$1

# Paths
openocd_path=$2
platform_path=$3
build_path=$4

# Board parameters
board_variant=$5
board_serial_num=$6
board_openocd_cfg=$7

# Project parameters
project_name=$8

# Optional arguments
verbose_flag=$9

function get_openocd_exe {
    openocd_exe="${openocd_path}/bin/openocd"
    # Depending on the OS, the openocd executable 
    # file extension changes.
    # Add ".exe" extension if the OS is Windows or cygwin
    if [[ "$OSTYPE" == "msys" ]]; then
        openocd_exe="${openocd_exe}.exe"
    fi
    echo ${openocd_exe}
}

function get_openocd_search_paths {
    # openocd config files search paths
    # A board support package might have additional openocd config files in its GeneratedSources path
    search_paths="-s ${openocd_path}/scripts -s ${platform_path}/mtb-libs/bsps/TARGET_APP_${board_variant}/config/GeneratedSource"
    echo ${search_paths}
}

function get_openocd_cmd {
    # openocd program verify reset exit command.
    # The target controller openocd configuration ${board_openocd_cfg} file is provided by the board.
    # The serial adapter number ${board_serial_num} is provided to ensure the right board is flashed in case of multiple KitProg3s connected.       
    openocd_cmd="-c \"source [find interface/kitprog3.cfg]; adapter serial ${board_serial_num}; source [find target/${board_openocd_cfg}]; psoc6 allow_efuse_program off; psoc6 sflash_restrictions 1; program ${build_path}/${project_name}.hex verify reset exit;\""
    echo ${openocd_cmd}
}

function get_openocd_log {
    # For quiet mode, the output is stored in a openocd.log file in the build path
    openocd_log="-l ${build_path}/${project_name}.openocd.log"
    if [[ ${verbose_flag} == "-v" ]]; then
        openocd_log=""
    fi
    echo ${openocd_log}
}

function get_upload_pattern {
    openocd_exe=$(get_openocd_exe)
    search_paths=$(get_openocd_search_paths)
    openocd_cmd=$(get_openocd_cmd)  
    openocd_log=$(get_openocd_log)
    upload_pattern="${openocd_exe} ${search_paths} ${openocd_cmd} ${openocd_log}"
    echo ${upload_pattern}
}

function print_upload_pattern {
    upload_pattern=$1
    echo "-----------------------------------------"
    echo "Openocd sketch upload pattern: "
    echo
    echo ${upload_pattern}
    echo
    echo "-----------------------------------------"
}

function parse_upload_err_code {
    error_code=$1

    # if error code is 0, the upload was successful
    if [[ ${error_code} == 0 ]]; then
        exit 0
    fi

    # Try to provide a more helpful error message
    # by inspecting the openocd log file
    openocd_log="${build_path}/${project_name}.openocd.log"

    # Search for the error message in the openocd log file
    error=$(grep "Error:" "${openocd_log}")

    # Known error messages
    cmsis_dap_device_not_found="Error: unable to find a matching CMSIS-DAP device"

    # Display user friendly message for kwnon errors
    # Otherwise, display the error message
    case ${error} in
        ${cmsis_dap_device_not_found})
            error_message="Are you sure the board is connected?"
            ;;
        *)
            error_message="${error}"
            ;;

    esac

    # Error message in red
    red="\e[31m"
    reset_color=$(tput sgr0)
    echo -e "${red}${error_message}${reset_color}"

    exit 1
}

function upload {
    upload_pattern=$(get_upload_pattern)
    # This will hide the openocd banner, 
    # which is even printer when using log file output
    hide_output="> /dev/null"

    if [[ ${verbose_flag} == "-v" ]]; then
        print_args
        print_upload_pattern "${upload_pattern}"
        hide_output=""
    fi

    eval ${upload_pattern} ${hide_output}

    parse_upload_err_code $?
}

function help {
    echo "Usage: "
    echo 
    echo "  bash upload.sh upload <openocd_path> <platform_path> <build_path> <board_variant> <board_serial_num> <board_openocd_cfg> <project_name> [-v]"
    echo "  bash upload.sh help"
    echo
    echo "Positional arguments for 'upload' command:"
    echo 
    echo "  openocd_path          Path to the openocd package directory"
    echo "  platform_path         Path to the platform directory"
    echo "  build_path            Path to the build directory"
    echo
    echo "  board_variant         Board variant (OPN name)"
    echo "  board_serial_num      Serial adapter number (KitProg3)"
    echo "  board_openocd_cfg     Board target controller openocd configuration file"
    echo
    echo "  project_name          Sketch .hex file name"

    echo "Optional arguments:"
    echo
    echo "  -v                    Verbose mode. Default is quiet mode which save output in log file."
}

function print_args {
    echo "-----------------------------------------"
    echo "Upload script arguments"
    echo
    echo "Paths"
    echo "-----"
    echo "openocd_path       ${openocd_path}"
    echo "platform_path      ${platform_path}"
    echo "build_path         ${build_path}"
    echo
    echo "Board parameters"
    echo "----------------"
    echo "board_variant      ${board_variant}"
    echo "board_serial_num   ${board_serial_num}"
    echo "board_openocd_cfg  ${board_openocd_cfg}"
    echo
    echo "Project parameters"
    echo "------------------"
    echo "project_name       ${project_name}"
    echo
    echo "Optional arguments"
    echo "------------------"
    echo "verbose_flag       ${verbose_flag}"
    echo
    echo "-----------------------------------------"
}

case ${upload_cmd} in
    "upload")
        upload
        ;;
   "help")
        help
        ;;
   *)
        help
        exit 1
        ;;
esac