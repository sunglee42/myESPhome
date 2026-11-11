"""Native frame ownership tests and optional real Espressif JPEG codec tests.

Set ESP_JPEG_SOURCE_DIR to the esp_jpeg directory from idf-extra-components
(version 1.3.2) to run the codec integration test. Only ESP platform plumbing
is shimmed; decoding uses the actual jpeg_decoder.c and Tiny JPEG Decoder.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

TESTS = Path(__file__).resolve().parent
COMPONENT = TESTS.parent
FLAGS = ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]


class FrameTests(unittest.TestCase):
    def test_queue_owns_frames_and_keeps_latest(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = str(Path(temp) / "queue")
            subprocess.run(["g++", "-std=c++17", *FLAGS, "-I", str(COMPONENT),
                            str(TESTS / "test_queue.cpp"), "-o", binary], check=True)
            subprocess.run([binary], check=True)

    @unittest.skipUnless(os.environ.get("ESP_JPEG_SOURCE_DIR"), "Set ESP_JPEG_SOURCE_DIR for real codec test")
    def test_real_codec_dimensions_color_and_bounds(self):
        from PIL import Image
        codec = Path(os.environ["ESP_JPEG_SOURCE_DIR"])
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            includes = ["-I", str(TESTS / "host_shims"), "-I", str(codec / "include"),
                        "-I", str(codec / "tjpgd"), "-I", str(COMPONENT)]
            # ESP's callback uses unsigned int where TJpgDec expects size_t.
            # They are identical on ESP32 (32-bit), but not on this 64-bit host.
            # Adapt only that callback signature in a temporary host copy.
            wrapper = temp / "jpeg_decoder.c"
            source_text = (codec / "jpeg_decoder.c").read_text()
            source_text = source_text.replace("static unsigned int jpeg_decode_in_cb(JDEC *jd, uint8_t *buff, unsigned int nbyte)",
                                              "static size_t jpeg_decode_in_cb(JDEC *jd, uint8_t *buff, size_t nbyte)")
            source_text = source_text.replace("static unsigned int jpeg_decode_in_cb(JDEC *dec, uint8_t *buff, unsigned int nbyte)",
                                              "static size_t jpeg_decode_in_cb(JDEC *dec, uint8_t *buff, size_t nbyte)")
            wrapper.write_text(source_text)
            objects = []
            for source in [wrapper, codec / "tjpgd/tjpgd.c",
                           codec / "jpeg_default_huffman_table.c"]:
                obj = temp / (source.name + ".o")
                subprocess.run(["gcc", *FLAGS, *includes, "-c", str(source), "-o", str(obj)], check=True)
                objects.append(str(obj))
            binary = str(temp / "jpeg")
            subprocess.run(["g++", "-std=c++17", *FLAGS, *includes,
                            str(COMPONENT / "usbuvc_version_jpeg.cpp"), str(TESTS / "test_jpeg.cpp"),
                            *objects, "-o", binary], check=True)
            paths = []
            for width, height in [(480, 320), (320, 240)]:
                path = temp / f"{width}.jpg"
                Image.new("RGB", (width, height), (255, 0, 0)).save(path, quality=85)
                paths.append(str(path))
            # UVC MJPEG commonly omits DHT; verify default Huffman fallback.
            data = Path(paths[0]).read_bytes()
            pos, no_dht = 2, bytearray(data[:2])
            while data[pos + 1] != 0xDA:
                length = int.from_bytes(data[pos + 2:pos + 4], "big")
                if data[pos + 1] != 0xC4:
                    no_dht.extend(data[pos:pos + 2 + length])
                pos += 2 + length
            no_dht.extend(data[pos:])
            missing_dht = temp / "no-dht.jpg"
            missing_dht.write_bytes(no_dht)
            subprocess.run([binary, *paths, str(missing_dht)], check=True)
