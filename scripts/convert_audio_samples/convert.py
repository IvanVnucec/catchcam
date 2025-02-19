import os
import subprocess

def convert_audio_samples_to_c(input_dir, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    for root, dirs, files in os.walk(input_dir):
        for file in files:
            input_file = os.path.join(root, file)
            output_file_raw = os.path.join(output_dir, "data.raw")
            subprocess.run([
                "ffmpeg",
                "-i", input_file,
                "-f", "s16le",
                "-acodec", "pcm_s16le",
                "-ar", "44100",
                "-ac", "1",
                "-y", output_file_raw
            ])

            with open(output_file_raw, "rb") as f:
                data = f.read()
            os.remove(output_file_raw)

            input_file_name = os.path.splitext(os.path.basename(input_file))[0]
            output_file = os.path.join(output_dir, f"audio_{input_file_name}_sample_data.h")
            with open(output_file, "w") as f:
                from datetime import datetime
                current_date = datetime.now().strftime("%d-%m-%Y %H:%M:%S")
                f.write(f'// This file was generated by a script on {current_date}\n\n')
                f.write("#pragma once\n\n")
                f.write("#include <stddef.h>\n")
                f.write("#include <stdint.h>\n")
                f.write("#include <pico/platform/sections.h>\n\n")

                # Convert raw data to 16-bit signed integers
                import struct
                samples = struct.unpack('<' + 'h' * (len(data) // 2), data)

                # Normalize samples to 16-bit signed integers
                max_sample = max(samples, key=abs)
                samples = [sample / max_sample for sample in samples]
                int16_min = -32768
                int16_max = 32767
                samples = [int((s * (int16_max - int16_min) / 2) + (int16_min + int16_max) / 2) for s in samples]
                assert all(int16_min <= sample <= int16_max for sample in samples)

                # Convert samples to hex strings
                data_list_str = ", ".join([f"{sample}" for sample in samples])

                output_file_name = os.path.splitext(os.path.basename(output_file))[0]
                var_name = output_file_name
                f.write(f'static const int16_t __in_flash("{var_name}") {var_name}[] = {{ {data_list_str} }};\n\n')
                f.write(f"static const size_t {var_name}_length = sizeof({var_name}) / sizeof({var_name}[0]);\n")

def main():
    script_dir = os.path.dirname(__file__)
    input_dir = os.path.join(script_dir, 'samples')
    output_dir = os.path.join(script_dir, 'converted')

    convert_audio_samples_to_c(input_dir, output_dir)

if __name__ == "__main__":
    main()
