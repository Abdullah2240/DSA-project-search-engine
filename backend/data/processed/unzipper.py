import gzip
import shutil

# Path to the .gz file
gz_path = "test.jsonl.gz"

# Path to the output file
output_path = "test.jsonl"

# Open and extract
with gzip.open(gz_path, 'rb') as f_in:
    with open(output_path, 'wb') as f_out:
        shutil.copyfileobj(f_in, f_out)

print(f"Extracted {gz_path} to {output_path}")
