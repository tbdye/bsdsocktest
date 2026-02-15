#!/usr/bin/env python3
"""
Generate bsdsocktest.info -- AmigaOS Workbench icon file.

Creates a standard old-style (OS 2.x/3.x compatible) WBTOOL icon with
4-color planar imagery, complement highlighting, and Tool Types.

Usage: python3 tools/mkicon.py [output_path]
       Default output: dist/bsdsocktest.info
"""

import struct
import sys

# --- AmigaOS constants ---

WB_DISKMAGIC = 0xE310
WB_DISKVERSION = 1
WB_DISKREVISION = 1
WBTOOL = 3
NO_ICON_POSITION = -2147483648  # 0x80000000 signed

# Gadget flags
GFLG_GADGIMAGE = 0x0004   # render as Image (not Border)
# Bits 0-1 = 00 = complement highlighting (GADGHCOMP)
GACT_RELVERIFY = 0x0001
GACT_IMMEDIATE = 0x0002
BOOLGADGET = 0x0001

# --- Icon parameters ---

WIDTH = 30
HEIGHT = 20
DEPTH = 2       # 4 colors
STACK_SIZE = 65536

TOOLTYPES = [
    "LOOPBACK",
]

# --- 4-color Workbench palette ---
# 0 = background (grey/blue)
# 1 = black (outlines, shadow)
# 2 = white (highlights)
# 3 = orange (accent)


def build_icon_pixels():
    """Build the icon as a 2D list of color indices (0-3).

    Design: A beveled card/document with three text lines
    and an orange checkmark in the lower right.
    """
    W = WIDTH
    img = [[0] * W for _ in range(HEIGHT)]

    # --- Card outline ---
    # Top edge: white highlight (row 1, cols 1-26)
    for x in range(1, 27):
        img[1][x] = 2

    # Bottom edge: black shadow (row 18, cols 1-26)
    for x in range(1, 27):
        img[18][x] = 1

    # Orange header bar (row 2)
    img[2][1] = 2  # white left corner
    for x in range(2, 26):
        img[2][x] = 3
    img[2][26] = 1  # black right corner

    # Orange footer bar (row 17)
    img[17][1] = 2  # white left corner
    for x in range(2, 26):
        img[17][x] = 3
    img[17][26] = 1  # black right corner

    # Interior rows (rows 3-16): white left, orange borders, black right
    for y in range(3, 17):
        img[y][1] = 2    # white left highlight
        img[y][2] = 3    # orange left border
        img[y][25] = 3   # orange right border
        img[y][26] = 1   # black right shadow

    # --- Text lines (black bars representing test output) ---
    for x in range(4, 17):       # 13 pixels wide (cols 4-16)
        img[4][x] = 1            # line 1
        img[6][x] = 1            # line 2
    for x in range(4, 14):       # 10 pixels wide (cols 4-13)
        img[8][x] = 1            # line 3 (shorter)

    # --- Checkmark (orange) ---
    # Right arm: diagonal from (col 21, row 10) down-left to vertex (col 16, row 15)
    for i in range(6):
        img[10 + i][21 - i] = 3
    # Left arm: (col 14, row 13) down-right to vertex
    img[13][14] = 3
    img[14][15] = 3
    # Vertex at (col 16, row 15) already set by right arm

    return img


def pixels_to_planes(img, width, height, depth):
    """Convert 2D pixel array to planar bitplane data (Amiga format).

    Returns bytes: plane 0 in full, then plane 1 in full, etc.
    Each row is word-aligned (padded to 16-bit boundary).
    """
    row_stride = ((width + 15) // 16) * 2  # bytes per row, word-aligned
    result = bytearray()

    for plane_idx in range(depth):
        for y in range(height):
            row_bytes = bytearray(row_stride)
            for x in range(width):
                color = img[y][x]
                if color & (1 << plane_idx):
                    row_bytes[x // 8] |= (1 << (7 - (x % 8)))
            result.extend(row_bytes)

    return bytes(result)


def serialize_string(s):
    """Serialize a string with 4-byte length prefix (includes NUL)."""
    encoded = s.encode('ascii') + b'\x00'
    return struct.pack('>I', len(encoded)) + encoded


def serialize_tooltypes(types):
    """Serialize a Tool Types array for the .info file."""
    # Header: (num_entries + 1) * 4  (pointer array size including NULL)
    data = struct.pack('>I', (len(types) + 1) * 4)
    for tt in types:
        data += serialize_string(tt)
    return data


def generate_info():
    """Generate the complete .info file as bytes."""
    img = build_icon_pixels()
    image_data = pixels_to_planes(img, WIDTH, HEIGHT, DEPTH)

    # --- DiskObject header (78 bytes) ---
    # Magic + Version (4 bytes)
    header = struct.pack('>HH', WB_DISKMAGIC, WB_DISKVERSION)

    # Gadget structure (44 bytes)
    gadget = struct.pack('>IhhhhHHHIIIiIHI',
        0,                              # NextGadget (NULL)
        0, 0,                           # LeftEdge, TopEdge
        WIDTH, HEIGHT,                  # Width, Height (hit-box)
        GFLG_GADGIMAGE,                 # Flags (image + complement)
        GACT_RELVERIFY | GACT_IMMEDIATE,  # Activation
        BOOLGADGET,                     # GadgetType
        1,                              # GadgetRender (non-zero = image present)
        0,                              # SelectRender (0 = no second image)
        0,                              # GadgetText (NULL)
        0,                              # MutualExclude
        0,                              # SpecialInfo (NULL)
        0,                              # GadgetID
        WB_DISKREVISION,                # UserData (revision in low byte)
    )

    # DiskObject fields after Gadget (30 bytes)
    do_fields = struct.pack('>BBIIiiII',
        WBTOOL,                         # do_Type
        0,                              # padding
        0,                              # do_DefaultTool (NULL, we ARE the tool)
        1,                              # do_ToolTypes (non-zero = array follows)
        NO_ICON_POSITION,               # do_CurrentX
        NO_ICON_POSITION,               # do_CurrentY
        0,                              # do_DrawerData (NULL)
        0,                              # do_ToolWindow (NULL)
    )

    # Stack size (4 bytes)
    do_stack = struct.pack('>I', STACK_SIZE)

    disk_object = header + gadget + do_fields + do_stack
    assert len(disk_object) == 78, f"DiskObject is {len(disk_object)} bytes, expected 78"

    # --- Image header (20 bytes) ---
    image_header = struct.pack('>hhhhhIBBI',
        0, 0,                           # LeftEdge, TopEdge
        WIDTH, HEIGHT, DEPTH,           # Width, Height, Depth
        1,                              # ImageData (non-zero = data follows)
        (1 << DEPTH) - 1,               # PlanePick (0x03 for 2 planes)
        0,                              # PlaneOnOff
        0,                              # NextImage (NULL)
    )
    assert len(image_header) == 20, f"Image header is {len(image_header)} bytes, expected 20"

    # --- Assemble ---
    output = bytearray()
    output.extend(disk_object)
    # No DrawerData (do_DrawerData == 0)
    output.extend(image_header)
    output.extend(image_data)
    # No second image (SelectRender == 0)
    # No DefaultTool string (do_DefaultTool == 0)
    output.extend(serialize_tooltypes(TOOLTYPES))

    return bytes(output)


def preview_icon():
    """Print ASCII art preview of the icon."""
    img = build_icon_pixels()
    chars = ['.', '#', 'W', 'O']  # bg, black, white, orange
    print(f"Icon preview ({WIDTH}x{HEIGHT}, {DEPTH}-bit depth):")
    for y, row in enumerate(img):
        line = ''.join(chars[c] for c in row)
        print(f"  {y:2d}: {line}")


if __name__ == '__main__':
    if '--preview' in sys.argv:
        preview_icon()
        sys.exit(0)

    output_path = 'dist/bsdsocktest.info'
    for arg in sys.argv[1:]:
        if not arg.startswith('-'):
            output_path = arg
            break

    info_data = generate_info()
    with open(output_path, 'wb') as f:
        f.write(info_data)
    print(f"Generated {output_path} ({len(info_data)} bytes)")
