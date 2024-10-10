# This script is used to parse the ModusToolbox build output files to extract build flags
# that needs to be used as well in the MicroPython sources build.

import argparse, json, os


def get_file_content(file):
    with open(file, "r") as f:
        f_content = f.read()

    return f_content


def get_ccxx_build_flags(cycompiler_file, out_path):
    # Extract the flags located between '-c' option and
    # the first (.rsp) response file
    # Warning: is the response file a proper delimiter (?)
    def find_flags_start(build_cmd_list):
        return build_cmd_list.index("-c") + 1  # next is the first element

    def find_flags_end(build_cmd_list):
        file_opt = [item for item in build_cmd_list if item.startswith("@")]
        return build_cmd_list.index(file_opt[0])

    build_cmd = get_file_content(cycompiler_file)
    build_cmd_list = build_cmd.split()

    start_idx = find_flags_start(build_cmd_list)
    end_idx = find_flags_end(build_cmd_list)

    ccxx_flags = build_cmd_list[start_idx:end_idx]
    print(*ccxx_flags)

    # Join the list into a single string with spaces between flags and write into a file
    joined_flags = " ".join(ccxx_flags)
    os.makedirs(out_path, exist_ok=True)
    with open(os.path.join(out_path, "compiler_flags.txt"), "w") as file:
        file.write(joined_flags)


def get_ld_linker_flags(cylinker_file, mtb_libs_path, out_path):
    # Get flags only until the linker script after the compiler argument -T
    def find_flags_start(link_cmd_list):
        gcc_cmd = [item for item in link_cmd_list if item.endswith("arm-none-eabi-g++")]
        return link_cmd_list.index(gcc_cmd[0]) + 1

    def find_flags_end(link_cmd_list):
        link_script_file_param = [
            item for item in link_cmd_list if item.startswith("-T")
        ]
        return link_cmd_list.index(link_script_file_param[0]) + 1

    def set_path_of_linker_script(
        link_cmd_list, inker_script_param_index, mtb_libs_path
    ):
        mtb_libs_path = mtb_libs_path.replace("\\", "/")
        link_cmd_list[inker_script_param_index] = (
            "-T "
            + str(mtb_libs_path)
            + "/"
            + link_cmd_list[inker_script_param_index][len("-T") :]
        )

    link_cmd = get_file_content(cylinker_file)
    link_cmd_list = link_cmd.split()

    start_idx = find_flags_start(link_cmd_list)
    end_idx = find_flags_end(link_cmd_list)
    set_path_of_linker_script(link_cmd_list, end_idx - 1, mtb_libs_path)

    ld_flags = link_cmd_list[start_idx:end_idx]
    print(*ld_flags)

    # Join the list into a single string with spaces between flags and write into a file
    joined_flags = " ".join(ld_flags)
    os.makedirs(out_path, exist_ok=True)
    with open(os.path.join(out_path, "linker_flags.txt"), "w") as file:
        file.write(joined_flags)


def get_inc_dirs(inc_dirs_file, mtb_libs_path, out_path):

    inc_list = get_file_content(inc_dirs_file)
    inc_list_list = inc_list.split()

    # Add the mtb-libs path to the include directories
    inc_list_with_updated_path = [
        inc_dir.replace("-I", "-I" + str(mtb_libs_path) + "/")
        for inc_dir in inc_list_list
    ]
    # If windows path, replace backslashes with forward slashes
    inc_list_with_updated_path = [
        inc_dir.replace("\\", "/") for inc_dir in inc_list_with_updated_path
    ]
    print(*inc_list_with_updated_path)

    # Join the list into a single string with spaces between flags and write into a file
    joined_inc_dirs = " ".join(inc_list_with_updated_path)
    os.makedirs(out_path, exist_ok=True)
    with open(os.path.join(out_path, "inc_dirs.txt"), "w") as file:
        file.write(joined_inc_dirs)


def parser():
    def main_parser_func(args):
        parser.print_help()

    def parser_get_ccxxflags_func(args):
        get_ccxx_build_flags(args.cycompiler_file, args.out_path)

    def parser_get_ldflags_func(args):
        get_ld_linker_flags(args.cylinker_file, args.mtb_libs_path, args.out_path)

    def parser_get_inc_func(args):
        get_inc_dirs(args.inclist_file, args.mtb_libs_path, args.out_path)

    parser = argparse.ArgumentParser(
        description="Utility to retrieve ModusToolbox build info"
    )
    subparser = parser.add_subparsers()
    parser.set_defaults(func=main_parser_func)

    parser_ccxx = subparser.add_parser(
        "ccxxflags", description="Get C and CXX build flags"
    )
    parser_ccxx.add_argument("cycompiler_file", type=str, help=".cycompiler file")
    parser_ccxx.add_argument("out_path", type=str, help="Path to output folder")
    parser_ccxx.set_defaults(func=parser_get_ccxxflags_func)

    parser_ld = subparser.add_parser("ldflags", description="Get linker flags")
    parser_ld.add_argument("cylinker_file", type=str, help=".cylinker file")
    parser_ld.add_argument("mtb_libs_path", type=str, help="Path to mtb-libs folder")
    parser_ld.add_argument("out_path", type=str, help="Path to output folder")
    parser_ld.set_defaults(func=parser_get_ldflags_func)

    parser_inc = subparser.add_parser("inc_dirs", description="Get include dirs -I")
    parser_inc.add_argument("inclist_file", type=str, help="inclist.rsp file")
    parser_inc.add_argument("mtb_libs_path", type=str, help="Path to mtb-libs folder")
    parser_inc.add_argument("out_path", type=str, help="Path to output folder")
    parser_inc.set_defaults(func=parser_get_inc_func)

    # Parser call
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    parser()
