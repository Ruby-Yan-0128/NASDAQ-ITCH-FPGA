import struct
import urllib.request
import zlib

url = "https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.gz"
target_bytes = 20_000_000  # 20 MB uncompressed (~6-8 MB downloaded)

decompressor = zlib.decompressobj(16 + zlib.MAX_WBITS)
raw_buf = bytearray()

with urllib.request.urlopen(url) as resp:
    while len(raw_buf) < target_bytes:
        chunk = resp.read(65536)
        if not chunk:
            break
        raw_buf.extend(decompressor.decompress(chunk))

# Trim cleanly to the last complete [2-byte length][payload] message
offset = 0
while offset + 2 <= len(raw_buf):
    msg_len = struct.unpack_from(">H", raw_buf, offset)[0]
    if msg_len == 0 or offset + 2 + msg_len > len(raw_buf):
        break
    offset += 2 + msg_len

with open("itch_sample.bin", "wb") as f:
    f.write(raw_buf[:offset])

print(f"Saved {offset:,} bytes of clean ITCH 5.0 messages.")