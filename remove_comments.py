import os
import re

def strip_comments(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    new_lines = []
    for line in lines:
        # If the line is purely a comment (with optional whitespace)
        if re.match(r'^\s*//.*', line):
            continue
        
        # If it has a trailing comment, remove it (careful with // inside strings or URLs, but we don't have them here)
        # Using a simple split by '//' for this basic C++ codebase, since it doesn't have complex string literals with //
        if '//' in line:
            parts = line.split('//')
            new_lines.append(parts[0].rstrip() + '\n')
        else:
            new_lines.append(line)
            
    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

def main():
    src_dir = 'src'
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith('.cpp') or file.endswith('.h'):
                strip_comments(os.path.join(root, file))

if __name__ == '__main__':
    main()
