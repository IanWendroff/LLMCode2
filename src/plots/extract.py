import os

input_folder = "../trial8"
output_file = "combined_output.txt"

with open(output_file, "w", encoding="utf-8") as outfile:
    for filename in os.listdir(input_folder):
        if filename.endswith(".csv"):
            file_path = os.path.join(input_folder, filename)
            
            try:
                with open(file_path, "r", encoding="utf-8") as infile:
                    lines = infile.readlines()
                    
                    if len(lines) >= 2:
                        second_line = lines[1].strip()
                        outfile.write(f"{filename}: {second_line}\n")
                    else:
                        outfile.write(f"{filename}: [No second line]\n")
            
            except Exception as e:
                outfile.write(f"{filename}: [Error reading file: {e}]\n")

print(f"Done! Output written to {output_file}")