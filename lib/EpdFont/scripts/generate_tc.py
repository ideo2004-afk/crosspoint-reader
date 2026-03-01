import sys

def main():
    chars = []
    # Big5 Level 1 range: High byte 0xA4-0xC6, Low byte 0x40-0x7E, 0xA1-0xFE
    for h in range(0xA4, 0xC6 + 1):
        for l in list(range(0x40, 0x7E + 1)) + list(range(0xA1, 0xFE + 1)):
            try:
                b = bytes([h, l])
                c = b.decode('big5')
                chars.append(c)
            except UnicodeDecodeError:
                pass
            
    # Add some common punctuations and missing ones just in case
    extra_chars = "「」『』【】（）《》〈〉⋯—ー"
    chars.extend(list(extra_chars))
    
    unique_chars = sorted(list(set(chars)))
    
    with open('tc_5401.txt', 'w', encoding='utf-8') as f:
        f.write(''.join(unique_chars))
        
    print(f"Generated {len(unique_chars)} characters into tc_5401.txt")

if __name__ == "__main__":
    main()
