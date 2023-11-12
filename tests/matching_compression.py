# Based on two input directories, uncompressed and compressed, compress the uncompressed directories's files using lzkn64 and compare them to the compressed directory's files.

import sys
import os
import subprocess
import filecmp
import shutil
import argparse
from colorama import Fore
from colorama import Style

parser = argparse.ArgumentParser(description='Compresses the files in the uncompressed directory and compares them to the files in the compressed directory.')
parser.add_argument("--uncompressed_dir", help="The directory containing the uncompressed files.", default="uncompressed")
parser.add_argument("--compressed_dir", help="The directory containing the compressed files.", default="compressed")
parser.add_argument("--temp_dir", help="The temporary directory to store the compressed files in.", default="recompressed")
parser.add_argument("--lzkn64", help="The path to the lzkn64 executable.", default="../build/lzkn64")
parser.add_argument("--verbose", help="Prints more information.", action="store_true")
args = parser.parse_args()

if __name__ == "__main__":
    test_successful = True
    nonmatching_files = []

    # Get the arguments.
    uncompressed_dir = args.uncompressed_dir
    compressed_dir = args.compressed_dir
    temp_dir = args.temp_dir
    lzkn64 = args.lzkn64
    verbose = args.verbose

    # Create a temporary directory to store the compressed files.
    if os.path.exists(temp_dir):
        shutil.rmtree(temp_dir)
    os.mkdir(temp_dir)

    # Compress the files in the uncompressed directory.
    for root, dirs, files in os.walk(uncompressed_dir):
        for file in files:
            # Create the output file path.
            output_file = os.path.join(temp_dir, os.path.relpath(os.path.join(root, file), uncompressed_dir))

            # Create the output directory if it doesn't exist.
            output_dir = os.path.dirname(output_file)
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)

            # Compress the file.
            subprocess.call([lzkn64, "-c", os.path.join(root, file), output_file])

    # Compare the compressed files to the files in the compressed directory.
    for root, dirs, files in os.walk(temp_dir):
        for file in files:
            # Create the output file path.
            output_file = os.path.join(compressed_dir, os.path.relpath(os.path.join(root, file), temp_dir))

            # Compare the files.
            if not filecmp.cmp(os.path.join(root, file), output_file):
                test_successful = False
                nonmatching_files.append(file)

    # Remove the temporary directory.
    shutil.rmtree(temp_dir)

    # Print the result.
    if test_successful:
        print(f"{Style.BRIGHT}{Fore.GREEN}Test successful!{Style.RESET_ALL}")
    else:
        print(f"{Style.BRIGHT}{Fore.RED}Test failed!{Style.RESET_ALL}")

        if verbose:
            print("Nonmatching files:") 
            for file in nonmatching_files:
                print(file)

    sys.exit(0 if test_successful else 1)