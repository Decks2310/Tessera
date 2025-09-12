import os

def extract_code_files(root_folder, output_file):
    """
    Extracts all .cpp and .h files from a folder and its subdirectories
    into a single text file.

    Args:
        root_folder (str): The path to the folder to search (e.g., "." for the current directory).
        output_file (str): The name of the text file to create (e.g., "combined_code.txt").
    """
    try:
        with open(output_file, 'w', encoding='utf-8') as outfile:
            print(f"Searching for .cpp and .h files in '{os.path.abspath(root_folder)}'...")

            file_count = 0
            # Walk through the directory tree
            for dirpath, _, filenames in os.walk(root_folder):
                for filename in filenames:
                    if filename.endswith(".cpp") or filename.endswith(".h"):
                        file_path = os.path.join(dirpath, filename)
                        
                        # Write the subdirectory and file name as a header
                        relative_path = os.path.relpath(file_path, root_folder)
                        outfile.write("=" * 80 + "\n")
                        outfile.write(f"// File: {relative_path.replace(os.sep, '/')}\n")
                        outfile.write("=" * 80 + "\n\n")
                        
                        # Write the content of the file
                        try:
                            with open(file_path, 'r', encoding='utf-8', errors='ignore') as infile:
                                outfile.write(infile.read())
                                outfile.write("\n\n")
                            file_count += 1
                        except Exception as e:
                            outfile.write(f"*** Error reading file: {file_path} ***\n")
                            outfile.write(f"*** Reason: {e} ***\n\n")
            
            print(f"Done! Extracted {file_count} files into '{output_file}'.")

    except Exception as e:
        print(f"An error occurred: {e}")

# --- Configuration ---
# Set the folder you want to search. "." means the current directory.
code_directory = "." 
# Set the desired name for the output text file
output_filename = "combined_code.txt"

# --- Run the script ---
if __name__ == "__main__":
    if not os.path.isdir(code_directory):
        print(f"Error: The folder '{code_directory}' was not found.")
    else:
        extract_code_files(code_directory, output_filename)
