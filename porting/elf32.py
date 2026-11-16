"""Inspect the ARM ELF files produced by this project without running them."""
import struct


def inspect(data):
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected little-endian ELF32")
    machine = struct.unpack_from("<H", data, 18)[0]
    flags = struct.unpack_from("<I", data, 36)[0]
    if machine != 40 or flags & 0x400 or flags >> 24 != 5:
        raise ValueError("Expected ARM EABI5 with the softfp calling convention")
    phoff, shoff = struct.unpack_from("<II", data, 28)
    phsize, phcount, shsize, shcount = struct.unpack_from("<HHHH", data, 42)
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shsize) for i in range(shcount)]

    def string_at(table, index):
        start = table[4] + index
        return data[start:data.index(b"\0", start)].decode("utf-8")

    needed, exported, interpreter = [], [], None
    for i in range(phcount):
        header = struct.unpack_from("<IIIIIIII", data, phoff + i * phsize)
        if header[0] == 3:
            interpreter = data[header[1]:header[1] + header[4]].rstrip(b"\0").decode()
    for section in sections:
        if section[1] == 6:  # SHT_DYNAMIC
            strings = sections[section[6]]
            for pos in range(section[4], section[4] + section[5], section[9]):
                tag, value = struct.unpack_from("<II", data, pos)
                if tag == 1:
                    needed.append(string_at(strings, value))
                if tag == 22 or (tag == 30 and value & 4):
                    raise ValueError("Unexpected text relocations")
        if section[1] == 11:  # SHT_DYNSYM
            strings = sections[section[6]]
            for pos in range(section[4], section[4] + section[5], section[9]):
                name, value, size, info, other, index = struct.unpack_from("<IIIBBH", data, pos)
                if index and (info >> 4) in (1, 2) and (other & 3) in (0, 3):
                    exported.append(string_at(strings, name))
    return {"machine": machine, "flags": hex(flags), "interpreter": interpreter,
            "needed": needed, "exports": exported}
