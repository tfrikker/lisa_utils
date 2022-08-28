password_checksum = [0x00, 0x00, 0x00, 0xF7, 0xC2, 0xE2, 0x00, 0x04, 0x1F, 0x62]
password = "Sh%P%y4Xjf3&^C8Kp#75"

if (len(password) <= 8):
    password_checksum[0] = len(password)
else:
    password_checksum[0] = 0x08

password_index = 1
i = 0
for c in password:
    ascii_val = ord(c)
    # special case for when password first hits 8 chars
    if (i == 8):
        password_checksum[9] = ascii_val
   
    password_checksum[password_index] = ascii_val + i
    
    
    i += 1
    password_index += 1
    if (password_index == 9):
        password_index = 1

print(''.join('{:02X}'.format(a) for a in password_checksum))

